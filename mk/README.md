# Makefile Layout

This directory contains Makefile fragments included by the top-level
`Makefile`.

Suggested include order:

1. `config.mk` - user-facing defaults and compatibility variables.
2. `toolchain.mk` - compiler, binutils, and host tool selection.
3. `arch-riscv.mk` - RISC-V compiler, linker, and QEMU settings.
4. `fs.mk` - filesystem feature selection and rootfs image policy.
5. `sources.mk` - source discovery and object list generation.
6. `build.mk` - kernel, user test, and object build rules.
7. `images.mk` - filesystem image and host mkfs rules.
8. `run.mk` - QEMU, debug, and run profile rules.
9. `checks.mk` - format, lint, compile database, and clang-tidy rules.
10. `clean.mk` - clean targets.

