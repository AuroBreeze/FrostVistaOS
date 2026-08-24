# QEMU run profiles and debugger entry points.

QEMUFLAGS := -machine virt -nographic $(QEMU_BOOT_FLAGS) -bios $(BUILD_DIR)/bootloader.bin

qemu:
	$(MAKE) clean ARCH=loongarch
	$(MAKE) bootloader ARCH=$(ARCH)
	$(MAKE) run ARCH=$(ARCH)

run: bootloader
	$(QEMU) $(QEMUFLAGS)
