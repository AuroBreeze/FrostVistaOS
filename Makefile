MAKEFLAGS += -j$(shell nproc)

# Set the default ARCH to riscv
ARCH ?= riscv
# Set the test file to run
TEST ?= argc

LOG_NUM ?= 2

ifeq ($(LOG), TRACE)
	LOG_NUM = 0
else ifeq ($(LOG), DEBUG)
	LOG_NUM = 1
else ifeq ($(LOG), INFO)
	LOG_NUM = 2
else ifeq ($(LOG), WARN)
	LOG_NUM = 3
else ifeq ($(LOG), ERROR)
	LOG_NUM = 4
endif

XXD = xxd

# Build type: release (optimized) or debug (symbols, no optimization)
BUILD ?= release
ifeq ($(BUILD), debug)
	OPT_FLAGS = -O0 -g
else
	OPT_FLAGS = -O2
endif

# Out-of-tree build directories
BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
GEN_DIR   := $(BUILD_DIR)/gen
TEST_DIR  := $(BUILD_DIR)/test

# Define the disk image name
DISK_IMG = $(BUILD_DIR)/disk.img
HOST_CC = gcc
MKFS_TOOL = $(BUILD_DIR)/mkfs_tool

ifeq ($(ARCH), riscv)
	CROSS = riscv64-elf
	ARCH_CFLAGS = -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany
	LINKER_SCRIPT = arch/$(ARCH)/linker.ld
	QEMU = qemu-system-riscv64
	QEMUFLAGS = -machine virt -nographic -bios none -kernel $(BUILD_DIR)/kernel.elf

	# Append VirtIO disk arguments
	# 1. '-drive': Configures the backend storage (the host file)
	# 2. '-device': Configures the frontend hardware exposed to the guest OS
	QEMUFLAGS += -drive file=$(DISK_IMG),if=none,format=raw,id=x0
	QEMUFLAGS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
	# enable virtio-mmio v1.1 support
	QEMUFLAGS += -global virtio-mmio.force-legacy=false
endif

CC     = $(CROSS)-gcc
DUMP   = $(CROSS)-objdump

# Set the default include path
# $(GEN_DIR) first so generated headers shadow any stale copies in include/
INCLUDES = -I$(GEN_DIR) -Iinclude -Iarch/$(ARCH)/include

CFLAGS = $(ARCH_CFLAGS) -nostdlib -nostartfiles -ffreestanding $(OPT_FLAGS) $(INCLUDES)
CFLAGS += -DCURRENT_LOG_LEVEL=$(LOG_NUM)

LDFLAGS = -T $(LINKER_SCRIPT)

# Obtain all common kernel C files
KERNEL_C := $(wildcard kernel/*/*.c)

# fs/*.c (such as easyfs)
KERNEL_C += $(wildcard kernel/fs/*/*.c)

# Get all architecture specific C/S files
ARCH_C := $(wildcard arch/$(ARCH)/*/*.c)
ARCH_S := $(wildcard arch/$(ARCH)/*/*.S)

OBJS := $(KERNEL_C:%.c=$(OBJ_DIR)/%.o) $(ARCH_C:%.c=$(OBJ_DIR)/%.o) $(ARCH_S:%.S=$(OBJ_DIR)/%.o)

# Generate the user test
USER_CFLAGS = $(ARCH_CFLAGS) -nostdlib -fno-builtin -ffreestanding -O2 -Itest
USER_LDFLAGS = -N -e _start -Ttext 0x10000

# Collect all source files for formatting (exclude generated/build files)
FORMAT_SRC := $(shell find kernel arch include mkfs test \
                -name '*.c' -o -name '*.h' \
                2>/dev/null)

.PHONY: all clean clean_disk run build_test disasm lint format qemu compdb tidy tidy-file debug gdb

build_test:
	@echo "Building user test: test/test_$(TEST).c"
	@mkdir -p $(TEST_DIR)
	$(CC) $(USER_CFLAGS) -c test/ulib.c -o $(TEST_DIR)/ulib.o
	$(CC) $(USER_CFLAGS) -c test/test_$(TEST).c -o $(TEST_DIR)/test.o
	$(CC) $(USER_CFLAGS) $(USER_LDFLAGS) $(TEST_DIR)/ulib.o $(TEST_DIR)/test.o -o $(TEST_DIR)/init_bin
	@mkdir -p $(GEN_DIR)/kernel
	$(XXD) -i $(TEST_DIR)/init_bin | sed 's/unsigned char build_test_init_bin/unsigned char init_elf/g' > $(GEN_DIR)/kernel/init_code.h
	@echo "Generated $(GEN_DIR)/kernel/init_code.h"

all:
	$(MAKE) build_test TEST=$(TEST)
	$(MAKE) $(BUILD_DIR)/kernel.elf
	cp $(BUILD_DIR)/kernel.elf kernel-rv

$(BUILD_DIR)/kernel.elf: $(OBJS) $(LINKER_SCRIPT)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

# Make 'run' depend on the disk image so it is created before QEMU starts
run: $(BUILD_DIR)/kernel.elf $(DISK_IMG)
	$(QEMU) $(QEMUFLAGS)

disasm: $(BUILD_DIR)/kernel.elf
	$(DUMP) -D -S -s $(BUILD_DIR)/kernel.elf > $(BUILD_DIR)/disasm.txt


$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# exec.o must be recompiled when the test payload changes
$(OBJ_DIR)/kernel/core/exec.o: $(GEN_DIR)/kernel/init_code.h

# Generate a 32MB raw disk image and format it with mkfs
$(MKFS_TOOL): mkfs/mkfs.c
	@echo "Building host tool: $(MKFS_TOOL)"
	@mkdir -p $(BUILD_DIR)
	$(HOST_CC) -O2 mkfs/mkfs.c -o $(MKFS_TOOL) -Iinclude

$(DISK_IMG): $(MKFS_TOOL) clean_disk
	@echo "Generating empty disk image: $@"
	dd if=/dev/zero of=$@ bs=1M count=32

	@echo "Formatting the disk image with your filesystem..."
	# Run the formatting tool on the freshly zeroed disk
	./$(MKFS_TOOL) $@ $(TEST_DIR)/init_bin


# Build and run QEMU with a freshly generated local disk image.
qemu:
	$(MAKE) clean
	$(MAKE) build_test TEST=$(TEST)
	$(MAKE) run

# Debug build: clean, rebuild with -O0 -g, start QEMU paused for GDB
#   Terminal 1: make debug TEST=init
#   Terminal 2: make gdb
debug:
	@$(MAKE) build_test TEST=$(TEST) BUILD=debug
	@$(MAKE) $(BUILD_DIR)/kernel.elf BUILD=debug
	@$(MAKE) $(DISK_IMG)
	@echo ""
	@echo "=== QEMU paused, waiting for GDB on :1234 ==="
	@echo "Run 'make gdb' in another terminal."
	@echo ""
	$(QEMU) $(QEMUFLAGS) -s -S

# Connect GDB to a waiting QEMU
gdb:
	$(CROSS)-gdb $(BUILD_DIR)/kernel.elf \
		-ex 'set confirm off' \
		-ex 'target remote :1234'

# Check if all source files comply with .clang-format (for CI)
lint:
	@echo "Checking code style..."
	@clang-format --dry-run -Werror $(FORMAT_SRC) && echo "All files formatted correctly."

# Reformat all source files in-place
format:
	@echo "Formatting source files..."
	@clang-format -i $(FORMAT_SRC)
	@echo "Formatting done."

# Generate compile_commands.json for clang-tidy / clangd
compdb:
	@echo "Generating compile_commands.json..."
	@{ \
		echo '['; \
		first=1; \
		for src in $(KERNEL_C) $(ARCH_C); do \
			[ "$$first" -eq 0 ] && echo ','; \
			first=0; \
			printf '  { "directory": "%s", "command": "%s %s -c %s", "file": "%s" }' \
				"$(CURDIR)" "$(CC)" "$(CFLAGS)" "$$src" "$$src"; \
		done; \
		echo ''; \
		echo ']'; \
	} > compile_commands.json
	@echo "Generated compile_commands.json"

# Run clang-tidy on all kernel source files
tidy: compdb
	@echo "Running clang-tidy..."
	@clang-tidy -p compile_commands.json $(KERNEL_C) $(ARCH_C) 2>&1
	@echo "Tidy done."

# Run clang-tidy on a single file:  make tidy-file FILE=kernel/fs/block_cache.c
tidy-file: compdb
	@clang-tidy -p compile_commands.json $(FILE)

# Separate target to clean the disk image, preventing accidental data loss
clean_disk:
	@echo "Cleaning disk image: $(DISK_IMG)"
	rm -f $(DISK_IMG)

clean: clean_disk
	rm -rf $(BUILD_DIR)
