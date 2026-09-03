# QEMU run profiles and debugger entry points.

QEMUFLAGS := -machine virt -m 128M -nographic $(QEMU_BOOT_FLAGS) \
	-kernel $(BUILD_DIR)/kernel.elf

# QEMUFLAGS += -drive file=$(ROOTFS_IMG),if=none,format=raw,id=x0
# QEMUFLAGS += -device virtio-blk-pci,drive=x0

.PHONY: qemu run

qemu:
	$(MAKE) clean ARCH=loongarch
	$(MAKE) build_test ARCH=loongarch TEST=$(TEST)
	$(MAKE) -B $(KERNEL_ELF) ARCH=loongarch BOOT=$(BOOT) \
		FS_LIST="$(FS_LIST)" ROOTFS=$(ROOTFS) BUILD=$(BUILD) TEST=$(TEST)
	$(MAKE) run ARCH=loongarch BOOT=$(BOOT) FS_LIST="$(FS_LIST)" \
		ROOTFS=$(ROOTFS) BUILD=$(BUILD) TEST=$(TEST)

run: check-direct-boot $(ROOTFS_DEPS)
	$(QEMU) $(QEMUFLAGS)

# Debug build: clean, rebuild with -O0 -g, start QEMU paused for GDB.
# Override GDB_PORT when the default port is already in use.
#   Terminal 1: make debug TEST=init
#   Terminal 2: make gdb
debug: $(ROOTFS_DEPS)
	@$(MAKE) build_test TEST=$(TEST)
	@$(MAKE) -B $(KERNEL_ELF) BUILD=debug BOOT=$(BOOT) FS_LIST="$(FS_LIST)" ROOTFS=$(ROOTFS) TEST=$(TEST)
	@echo ""
	@echo "=== QEMU paused, waiting for GDB on :$(GDB_PORT) ==="
	@echo "Run 'make gdb' in another terminal."
	@echo ""
	$(QEMU) $(QEMUFLAGS) -gdb tcp::$(GDB_PORT) -S
