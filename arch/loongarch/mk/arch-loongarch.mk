# LoongArch architecture configuration.
#
# Consumes:
#   ARCH, BOOT
#
# Produces:
#   ARCH_CFLAGS, BOOT_CFLAGS, LINKER_SCRIPT, ARCH_EXCLUDE_C,
#   ARCH_EXCLUDE_S, QEMU

QEMU = qemu-system-loongarch64
ARCH_CFLAGS = -march=la464 -mabi=lp64d

ifeq ($(origin CROSS), undefined)
  ifneq ($(shell command -v loongarch64-unknown-linux-gnu-gcc 2>/dev/null),)
    CROSS := loongarch64-unknown-linux-gnu
  else ifneq ($(shell command -v loongarch64-elf-gcc 2>/dev/null),)
    CROSS := loongarch64-elf
  else
    $(error LoongArch toolchain not found. Set CROSS=...)
  endif
endif

ifeq ($(BOOT), bare)
  BOOT_CFLAGS :=
  LINKER_SCRIPT := arch/$(ARCH)/linker.ld
  QEMU_BOOT_FLAGS := -bios none
  ARCH_EXCLUDE_C :=
  ARCH_EXCLUDE_S :=
else
  $(error Unsupported BOOT=$(BOOT). Use BOOT=bare or BOOT=opensbi)
endif
