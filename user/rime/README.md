# rime

rime is a small userspace C library for FrostVistaOS, extracted from the existing
implementations in `user/ulib.c`.

- `string.c`: string and memory operations.
- `crt.c`: the shared `_start` entry point, which calls `main(argc, argv)` and
  passes its return value to `exit`.
- `stdio.c`: `printf` and the project-specific `print` helper.
- `unistd.c`: `sbrk`, `execv`, `sleep`, and `unlink`, built on syscall wrappers.
- Public declarations live in `include/user/rime/` and are included by
  `user/user.h`.

`user/ulib.c` retains the wrappers that invoke the kernel directly, along with
FrostVistaOS-specific interfaces such as `shutdown` and terminal control.
Syscall wrappers are also typically part of a libc; for now, this file remains
the lower-level interface used by rime while the library is organized gradually.

Test programs for both architectures and RISC-V userspace applications compile
and link rime through `mk/rime.mk`. New `.c` files added to `user/rime/` are
automatically included in the build.

The initial extraction preserves the existing interfaces and behavior; rime
does not yet provide full ISO C or POSIX compatibility. For example, `printf`
returns `void` and supports `%d`, `%x`, `%p`, `%s`, `%c`, `%%`, and `%lu`.
Long output is written in chunks as the buffer fills. `sleep` still uses the
existing simplified implementation.
