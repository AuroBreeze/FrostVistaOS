# RISC-V architecture configuration.
#
# Consumes:
#   ARCH, BOOT, KERNEL_ELF, ROOTFS_IMG
#
# Produces:
#   ARCH_CFLAGS, BOOT_CFLAGS, LINKER_SCRIPT, ARCH_EXCLUDE_C,
#   ARCH_EXCLUDE_S, QEMU, QEMUFLAGS

ARCH_CFLAGS = -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany
QEMU = qemu-system-riscv64

ifeq ($(origin CROSS), undefined)
	ifneq ($(shell command -v riscv64-elf-gcc 2>/dev/null),)
    CROSS := riscv64-elf
  else ifneq ($(shell command -v riscv64-unknown-elf-gcc 2>/dev/null),)
    CROSS := riscv64-unknown-elf
  else
    CROSS := riscv64-linux-gnu
  endif
endif

ifeq ($(BOOT), opensbi)
  BOOT_CFLAGS := -DOPEN_SBI_BOOT
  LINKER_SCRIPT := arch/$(ARCH)/linker-sbi.ld
  QEMU_BOOT_FLAGS := -bios default
  ARCH_EXCLUDE_C := arch/$(ARCH)/boot/mstart.c arch/$(ARCH)/trap/mtrap.c
  ARCH_EXCLUDE_S := arch/$(ARCH)/trap/mtrapvec.S
else ifeq ($(BOOT), bare)
  BOOT_CFLAGS :=
  LINKER_SCRIPT := arch/$(ARCH)/linker.ld
  QEMU_BOOT_FLAGS := -bios none
  ARCH_EXCLUDE_C :=
  ARCH_EXCLUDE_S :=
else
  $(error Unsupported BOOT=$(BOOT). Use BOOT=bare or BOOT=opensbi)
endif
