# Kernel, user test, and object build rules.

ifeq ($(CONFIG_SLAB_BASELINE_TEST),Y)
CFLAGS += -DCONFIG_SLAB_BASELINE_TEST
endif

disasm: $(BUILD_DIR)/kernel.elf
	$(DUMP) -D -S -s $(BUILD_DIR)/kernel.elf > $(BUILD_DIR)/disasm.txt
