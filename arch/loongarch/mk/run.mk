# QEMU run profiles and debugger entry points.

QEMUFLAGS := -machine virt -nographic $(QEMU_BOOT_FLAGS) -kernel $(KERNEL_ELF)

qemu:
	$(MAKE) clean ARCH=loongarch
	$(MAKE) $(KERNEL_ELF) ARCH=$(ARCH)
	$(MAKE) run ARCH=$(ARCH)

run: $(KERNEL_ELF)
	$(QEMU) $(QEMUFLAGS)
