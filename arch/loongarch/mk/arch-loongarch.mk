# LoongArch architecture configuration.
#
# Consumes:
#   ARCH, BOOT
#
# Produces:
#   ARCH_CFLAGS, BOOT_CFLAGS, LINKER_SCRIPT, ARCH_EXCLUDE_C,
#   ARCH_EXCLUDE_S, QEMU

QEMU = qemu-system-loongarch64
# The early boot path does not enable LSX/LASX, so keep the kernel scalar.
# Use the individual switches for compatibility with older GCC toolchains.
ARCH_CFLAGS = -march=la464 -mabi=lp64d -mcmodel=normal -mno-lsx -mno-lasx

# An environment CROSS from another architecture must not leak into this
# architecture. Keep explicit command-line and environment overrides intact.
ifneq ($(filter undefined environment,$(origin CROSS)),)
  ifneq ($(shell command -v loongarch64-unknown-linux-gnu-gcc 2>/dev/null),)
    CROSS := loongarch64-unknown-linux-gnu
  else ifneq ($(shell command -v loongarch64-elf-gcc 2>/dev/null),)
    CROSS := loongarch64-elf
  else ifeq ($(filter compdb,$(MAKECMDGOALS)),compdb)
    # compdb only records the command; the compiler need not run on the host.
    CROSS := loongarch64-unknown-linux-gnu
  else
    $(error LoongArch toolchain not found. Set CROSS=...)
  endif
endif

ifeq ($(BOOT), bare)
  BOOT_CFLAGS :=
  LINKER_SCRIPT := arch/$(ARCH)/linker.ld
  QEMU_BOOT_FLAGS :=
  ARCH_EXCLUDE_C :=
  ARCH_EXCLUDE_S :=
else
  $(error Unsupported BOOT=$(BOOT). Use BOOT=bare)
endif
