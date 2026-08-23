# Kernel, user test, and object build rules.

disasm: $(BUILD_DIR)/kernel.elf
	$(DUMP) -D -S -s $(BUILD_DIR)/kernel.elf > $(BUILD_DIR)/disasm.txt
