# QEMU run profiles and debugger entry points.

QEMUFLAGS := -machine virt -m 128M -nographic $(QEMU_BOOT_FLAGS) \
	-kernel $(BUILD_DIR)/kernel.elf

QEMUFLAGS += -drive file=$(ROOTFS_IMG),if=none,format=raw,id=x0
QEMUFLAGS += -device virtio-blk-pci,drive=x0

.PHONY: qemu run

qemu:
	$(MAKE) clean ARCH=loongarch
	$(MAKE) run ARCH=loongarch

run: check-direct-boot $(ROOTFS_DEPS)
	$(QEMU) $(QEMUFLAGS)
