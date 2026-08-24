# QEMU run profiles and debugger entry points.

QEMUFLAGS := -machine virt -m 128M -nographic $(QEMU_BOOT_FLAGS) \
	-kernel $(BUILD_DIR)/kernel.elf

.PHONY: qemu run

qemu:
	$(MAKE) clean ARCH=loongarch
	$(MAKE) run ARCH=loongarch

run: check-direct-boot
	$(QEMU) $(QEMUFLAGS)
