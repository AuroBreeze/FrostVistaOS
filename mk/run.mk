# QEMU run profiles and debugger entry points.
#
# Consumes:
#   QEMU, QEMUFLAGS, KERNEL_ELF, ROOTFS_DEPS, BOOT, ROOTFS,
#   FS_LIST, TEST, CROSS, EXT4_IMG, CONTEST_MEM, CONTEST_SMP
#
# Produces targets:
#   run, run-sbi, run-sbi-ext4, run-contest-rv, qemu, debug, gdb

# Make 'run' depend on the disk image so it is created before QEMU starts

QEMUFLAGS := -machine virt -nographic $(QEMU_BOOT_FLAGS) -kernel $(KERNEL_ELF)
# Append VirtIO disk arguments
# 1. '-drive': Configures the backend storage (the host file)
# 2. '-device': Configures the frontend hardware exposed to the guest OS
QEMUFLAGS += -drive file=$(ROOTFS_IMG),if=none,format=raw,id=x0
QEMUFLAGS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
# enable virtio-mmio v1.1 support
QEMUFLAGS += -global virtio-mmio.force-legacy=false

# Build and run QEMU with a freshly generated local disk image.
qemu:
	$(MAKE) clean
	$(MAKE) build_test TEST=$(TEST)
	$(MAKE) run BOOT=$(BOOT) FS_LIST="easyfs devtmpfs" ROOTFS=easyfs

run: $(KERNEL_ELF) $(ROOTFS_DEPS)
	$(QEMU) $(QEMUFLAGS)

run-sbi:
	$(MAKE) run BOOT=opensbi
# Contest-style local probe: mount the official EXT4 test image as x0.
# This keeps the Easy-FS run target unchanged while the EXT4 reader is built.
run-sbi-ext4:
	$(MAKE) run BOOT=opensbi ROOTFS=ext4 FS_LIST="ext4"

# Debug build: clean, rebuild with -O0 -g, start QEMU paused for GDB
#   Terminal 1: make debug TEST=init
#   Terminal 2: make gdb
debug: $(ROOTFS_DEPS)
	@$(MAKE) build_test TEST=$(TEST)
	@$(MAKE) $(KERNEL_ELF) BUILD=debug BOOT=$(BOOT) FS_LIST="$(FS_LIST)" ROOTFS=$(ROOTFS)
	@echo ""
	@echo "=== QEMU paused, waiting for GDB on :1234 ==="
	@echo "Run 'make gdb' in another terminal."
	@echo ""
	$(QEMU) $(QEMUFLAGS) -s -S

# Connect GDB to a waiting QEMU
gdb:
	$(CROSS)-gdb $(BUILD_DIR)/kernel.elf \
		-ex 'set confirm off' \
		-ex 'target remote :1234'
