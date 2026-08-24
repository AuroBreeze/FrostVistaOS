# Kernel, user test, and object build rules.
#
# Consumes:
# 	GEN_DIR, CC, LDFLAGS , OBJS, LINKER_SCRIPT, ARCH
# 	OPT_FLAGS, LOG_NUM, CONFIG_TEST
#
# Produces:
# 	kernel.elf

INCLUDES = -I$(GEN_DIR) -Iinclude -Iarch/$(ARCH)/include
LDFLAGS = -T $(LINKER_SCRIPT)
OBJCOPY = $(CROSS)-objcopy
LD = $(CROSS)-ld


CFLAGS = $(ARCH_CFLAGS) -nostdlib -nostartfiles -ffreestanding $(OPT_FLAGS) $(INCLUDES)
CFLAGS += -fno-unwind-tables -fno-asynchronous-unwind-tables
# if log
CFLAGS += -DCURRENT_LOG_LEVEL=$(LOG_NUM)

ifeq ($(CONFIG_TEST),Y)
	 CFLAGS += -DCONFIG_TEST
endif


$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.elf: $(OBJS) $(LINKER_SCRIPT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	@mkdir -p $(dir $@)
	$(OBJCOPY) -O binary $< $@

KERNEL_BLOB := $(BUILD_DIR)/kernel_blob.o
BOOTLOADER_OBJ_DIR := $(BUILD_DIR)/bootloader-obj
BOOTLOADER_OBJS := $(BOOTLOADER_OBJ_DIR)/start.o $(BOOTLOADER_OBJ_DIR)/boot.o
BOOTLOADER_ELF := $(BUILD_DIR)/bootloader.elf
BOOTLOADER_BIN := $(BUILD_DIR)/bootloader.bin

$(KERNEL_BLOB): $(BUILD_DIR)/kernel.bin
	$(LD) -r -b binary -m elf64loongarch $< -o $@
	$(OBJCOPY) \
		--redefine-sym _binary_build_loongarch_kernel_bin_start=_kernel_blob_start \
		--redefine-sym _binary_build_loongarch_kernel_bin_end=_kernel_blob_end \
		--redefine-sym _binary_build_loongarch_kernel_bin_size=_kernel_blob_size \
		$@ $@.renamed
	mv $@.renamed $@

$(BOOTLOADER_OBJ_DIR)/%.o: arch/loongarch/bootloader/%.c
	@mkdir -p $(dir $@)
	$(CC) $(ARCH_CFLAGS) -nostdlib -nostartfiles -ffreestanding \
		-fno-unwind-tables -fno-asynchronous-unwind-tables -O2 -c $< -o $@

$(BOOTLOADER_OBJ_DIR)/%.o: arch/loongarch/bootloader/%.S
	@mkdir -p $(dir $@)
	$(CC) $(ARCH_CFLAGS) -nostdlib -nostartfiles -ffreestanding -c $< -o $@

$(BOOTLOADER_ELF): $(BOOTLOADER_OBJS) $(KERNEL_BLOB) arch/loongarch/bootloader/linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(ARCH_CFLAGS) -nostdlib -nostartfiles -ffreestanding \
		$(BOOTLOADER_OBJS) $(KERNEL_BLOB) \
		-T arch/loongarch/bootloader/linker.ld -o $@

$(BOOTLOADER_BIN): $(BOOTLOADER_ELF)
	$(OBJCOPY) -O binary $< $@

bootloader: $(BOOTLOADER_BIN)
