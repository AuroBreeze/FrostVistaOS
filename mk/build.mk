# Kernel, user test, and object build rules.
#
# Consumes:
#   CC, CFLAGS, LDFLAGS, KERNEL_ELF, OBJS, LINKER_SCRIPT,
#   TEST, TEST_DIR, GEN_DIR, USER_CFLAGS, USER_LDFLAGS, XXD
#
# Produces targets:
#   all, build_test, $(KERNEL_ELF), $(OBJ_DIR)/%.o

# Set the default include path
# $(GEN_DIR) first so generated headers shadow any stale copies in include/
INCLUDES = -I$(GEN_DIR) -Iinclude -Iarch/$(ARCH)/include

LDFLAGS = -T $(LINKER_SCRIPT)

# Generate the user test
USER_CFLAGS = $(ARCH_CFLAGS) -nostdlib -fno-builtin -ffreestanding -O2 -Itest
USER_LDFLAGS = -N -e _start -Ttext 0x10000


CFLAGS = $(ARCH_CFLAGS) -nostdlib -nostartfiles -ffreestanding $(OPT_FLAGS) $(INCLUDES)
# if log
CFLAGS += -DCURRENT_LOG_LEVEL=$(LOG_NUM)
# if boot
CFLAGS += $(BOOT_CFLAGS)
#if fs
CFLAGS += $(FS_CFLAGS) $(ROOTFS_CFLAGS)


$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build_test:
	@echo "Building user test: test/test_$(TEST).c"
	@mkdir -p $(TEST_DIR)
	$(CC) $(USER_CFLAGS) -c test/ulib.c -o $(TEST_DIR)/ulib.o
	$(CC) $(USER_CFLAGS) -c test/test_$(TEST).c -o $(TEST_DIR)/test.o
	$(CC) $(USER_CFLAGS) $(USER_LDFLAGS) $(TEST_DIR)/ulib.o $(TEST_DIR)/test.o -o $(TEST_DIR)/init_bin
	@echo "Generated $(TEST_DIR)/init_bin"
	@echo "Embedding $(TEST_DIR)/init_bin as /init via $(GEN_DIR)/kernel/init_code.h"
	@mkdir -p $(GEN_DIR)/kernel
	@if command -v $(XXD) >/dev/null 2>&1; then \
		$(XXD) -i -n init_code $(TEST_DIR)/init_bin > $(GEN_DIR)/kernel/init_code.h; \
	else \
		{ \
			echo 'unsigned char init_code[] = {'; \
			od -An -v -tx1 $(TEST_DIR)/init_bin | awk '{ for (i = 1; i <= NF; i++) printf "  0x%s,\n", $$i }'; \
			echo '};'; \
			bytes=$$(wc -c < $(TEST_DIR)/init_bin); \
			printf 'unsigned int init_code_len = %s;\n' "$$bytes"; \
		} > $(GEN_DIR)/kernel/init_code.h; \
	fi
	@echo "Generated $(GEN_DIR)/kernel/init_code.h"

all:
	$(MAKE) build_test TEST=$(TEST)
	$(MAKE) -B $(BUILD_DIR)/kernel.elf BOOT=opensbi FS_LIST="ext4 devtmpfs" ROOTFS=ext4 TEST=runner
	cp $(BUILD_DIR)/kernel.elf kernel-rv

$(BUILD_DIR)/kernel.elf: $(OBJS) $(LINKER_SCRIPT)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
	
disasm: $(BUILD_DIR)/kernel.elf
	$(DUMP) -D -S -s $(BUILD_DIR)/kernel.elf > $(BUILD_DIR)/disasm.txt
