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

# Define the disk image name
DISK_IMG = disk.img
HOST_CC = gcc
MKFS_TOOL = mkfs_tool

ifeq ($(ARCH), riscv)
	CROSS = riscv64-elf
	ARCH_CFLAGS = -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany
	LINKER_SCRIPT = arch/$(ARCH)/linker.ld
	QEMU = qemu-system-riscv64
	QEMUFLAGS = -machine virt -nographic -bios none -kernel kernel.elf
	
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
INCLUDES = -Iinclude -Iarch/$(ARCH)/include

CFLAGS = $(ARCH_CFLAGS) -nostdlib -nostartfiles -ffreestanding -O2 $(INCLUDES)
CFLAGS += -DCURRENT_LOG_LEVEL=$(LOG_NUM)

LDFLAGS = -T $(LINKER_SCRIPT)

# Obtain all common kernel C files
KERNEL_C := $(wildcard kernel/*/*.c)

# Get all architecture specific C/S files
ARCH_C := $(wildcard arch/$(ARCH)/*/*.c)
ARCH_S := $(wildcard arch/$(ARCH)/*/*.S)

OBJS := $(KERNEL_C:.c=.o) $(ARCH_C:.c=.o) $(ARCH_S:.S=.o)

# Generate the user test
USER_CFLAGS = $(ARCH_CFLAGS) -nostdlib -fno-builtin -ffreestanding -O2 -Itest
USER_LDFLAGS = -N -e _start -Ttext 0x10000

.PHONY: all clean clean_disk run build_test

build_test:
	@echo "Building user test: test/test_$(TEST).c"
	$(CC) $(USER_CFLAGS) -c test/ulib.c -o test/ulib.o
	$(CC) $(USER_CFLAGS) -c test/test_$(TEST).c -o test/test.o
	$(CC) $(USER_CFLAGS) $(USER_LDFLAGS) test/ulib.o test/test.o -o test/init_bin
	$(XXD) -i test/init_bin | sed 's/unsigned char test_init_bin/unsigned char init_elf/g' > include/kernel/init_code.h
	@echo "Generated include/kernel/init_code.h"

all:
	$(MAKE) clean
	$(MAKE) build_test TEST=$(TEST)
	$(MAKE) kernel.elf
	$(MAKE) run

disasm: kernel.elf
	$(DUMP) -dS kernel.elf > disasm.txt

kernel.elf: $(OBJS) $(LINKER_SCRIPT)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# Generate a 32MB raw disk image and format it with mkfs
$(MKFS_TOOL): mkfs/mkfs.c
	@echo "Building host tool: $(MKFS_TOOL)"
	$(HOST_CC) -O2 mkfs/mkfs.c -o $(MKFS_TOOL) $(INCLUDES)

$(DISK_IMG): $(MKFS_TOOL) clean_disk
	@echo "Generating empty disk image: $@"
	dd if=/dev/zero of=$@ bs=1M count=32
	
	@echo "Formatting the disk image with your filesystem..."
	# Run the formatting tool on the freshly zeroed disk
	./$(MKFS_TOOL) $@

# Make 'run' depend on the disk image so it is created before QEMU starts
run: kernel.elf $(DISK_IMG)
	$(QEMU) $(QEMUFLAGS)

# Separate target to clean the disk image, preventing accidental data loss
clean_disk:
	@echo "Cleaning disk image: $(DISK_IMG)"
	rm -f $(DISK_IMG)

clean: clean_disk
	rm -f $(OBJS) kernel.elf disasm.txt
	rm -f test/*.o test/init_bin include/kernel/init_code.h
	rm -f $(MKFS_TOOL)

