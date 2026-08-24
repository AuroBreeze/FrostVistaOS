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

.PHONY: check-direct-boot

check-direct-boot: $(BUILD_DIR)/kernel.elf
	sh scripts/check_loongarch_direct_boot.sh $< $(CROSS)-readelf
