# FrostVista OS / 霜见内核

```text
[   0.094] [ INFO] Paging enable successfully
------------------------------------------------------------
    ______                __ _    ___      __       
   / ____/________  _____/ /| |  / (_)____/ /_____ _
  / /_  / ___/ __ \/ ___/ __/ | / / / ___/ __/ __ `/
 / __/ / /  / /_/ (__  ) /_ | |/ / (__  ) /_/ /_/ / 
/_/   /_/   \____/____/\__/ |___/_/____/\__/\__,_/

RISC-V 64  |  Sv39  |  v1.0
------------------------------------------------------------
[   0.101] [ INFO] Enable time interrupts...
[   0.102] [ INFO] Timer init done
------------------------------------------------------------
  ◆ Platform Init
[   0.104] [ INFO] kalloc_init start
[   0.672] [ INFO] Total Memory Pages: 32039
[   0.673] [ INFO] kalloc_init end
[   0.673] [ INFO] clear low memory mappings
[   0.674] [ INFO] clear low memory mappings done
[   0.675] [ INFO] Hello FrostVista OS!
------------------------------------------------------------
  ◆ Process Subsystem
------------------------------------------------------------
  ◆ Filesystem & Devices
[   0.679] [ INFO] virtio-blk initialized, mmio version 2
------------------------------------------------------------
```

FrostVista is a compact **RISC-V 64 (Sv39)** kernel shaped by a simple idea:
keep the system small, but let every boundary be real.

It is built for learning, experimentation, and small embedded-style
environments. The kernel favors clear structure, direct code, and working
system paths over broad compatibility or unnecessary abstraction.

---

## Project Layout

```text
arch/riscv/        RISC-V boot, trap, paging, SBI, UART, timer, and PLIC code
kernel/core/       Process, syscall, exec, file descriptor, pipe, and scheduler paths
kernel/fs/         VFS plus Easy-FS, EXT4 read-only, devtmpfs, and block cache layers
kernel/driver/     VirtIO block device driver
include/           Kernel headers and shared constants
mk/                Makefile fragments for toolchain, sources, images, run profiles, and checks
mkfs/              Host Easy-FS image builder
scripts/           Test runner and helper scripts
test/              User-mode test entry programs; each `test/test_*.c` can become `/init`
user/              Shared user-mode runtime (`user.h`, `ulib.c`)
user/bin/          User applications packaged into Easy-FS, such as `echo`, `cat`, and `fvsh`
devlog/            Development notes
.senior-brother/   Project reasoning index used for navigation and future debugging
```

The test/application split is intentional:

```text
test/test_$(TEST).c  -> build/test/init_bin -> guest /init
user/bin/*.c         -> build/user/<app>    -> guest /<app>
```

`test/` programs are test entrypoints. `user/bin/` programs are normal user applications placed in the Easy-FS image. The shared user runtime lives in `user/user.h` and `user/ulib.c`.

---

## Roadmap

See [`releases.md`](./releases.md) for the active roadmap, milestone history, validation commands, and known follow-up work.

## FrostVista Shell (`fvsh`)

`fvsh` is a small interactive shell for exercising FrostVista's process, file descriptor, Easy-FS, and pipe paths. It is intentionally not a full POSIX shell.

Supported basics:

```text
help
pwd
cd /
exit
echo hello
cat file
echo hello > out
cat < out
echo hello | cat
echo hello | cat > out
```

Current limitations:

```text
No quotes or escapes:        echo "hello world"
No append redirection:       echo hi >> out
No stderr redirection:       cmd 2> err
No multi-stage pipelines:    a | b | c
No globbing or variables:    echo $HOME, ls *.c
No job control/background:   cmd &, fg, bg
No PATH/env search model:    user apps are packaged directly in Easy-FS
```

## Build & Run

**Requirements:**

* `riscv64-elf-gcc` (or similar cross-compiler)
* `qemu-system-riscv64`
* `make`

**To build and launch QEMU with the default interactive shell configuration:**

```bash
make qemu ROOTFS=easyfs FS_LIST="easyfs devtmpfs" TEST=fvsh
```

You should see the kernel enabling paging, mounting Easy-FS/devtmpfs, and starting the FrostVista shell (`fvsh`) in the serial console. The `qemu` target respects explicit build parameters, so use it as the normal hand-written run entry point.

The Easy-FS image is built by `mkfs/mkfs.c`. For shell runs it contains `/init` from the selected test plus packaged user applications from `user/bin/`, currently including `/echo`, `/cat`, and `/fvsh`.

Useful parameters:

```text
BOOT=bare|opensbi
ROOTFS=easyfs|ext4
FS_LIST="easyfs devtmpfs"|"ext4 devtmpfs"
TEST=<test name under test/test_*.c, without the test_ prefix>
BUILD=release|debug
```

Manual/demo tests such as `fvsh`, `init`, and `echo` are not part of the automated test list. Use `fvsh_script` for automated shell regression.

For the OpenSBI EXT4 runner path:

```bash
make qemu BOOT=opensbi ROOTFS=ext4 FS_LIST="ext4 devtmpfs" TEST=runner BUILD=debug
```

For a paused GDB session on the same path:

```bash
make debug BOOT=opensbi ROOTFS=ext4 FS_LIST="ext4 devtmpfs" TEST=runner
make gdb
```

## Automated Tests

The Python runner builds one user test at a time, launches QEMU, records logs under `logs/`, and classifies kernel diagnostics. Expected diagnostics from negative syscall tests are reported as `PASS_EXPECTED_LOG`; unexpected `[WARN]` or `[ERROR]` lines are surfaced separately. `sys_pipe` also count-limits its expected diagnostics so extra matching warnings are not silently accepted.

```bash
python3 ./scripts/run_tests.py --list
python3 ./scripts/run_tests.py -t fvsh_script -T 30
python3 ./scripts/run_tests.py -t sys_pipe -T 20 --skip-kernel
python3 ./scripts/run_tests.py -t easyfs -T 20 --skip-kernel
python3 ./scripts/run_tests.py -t backend -T 20 --skip-kernel --rootfs ext4 --fs-list "ext4 devtmpfs"
python3 ./scripts/run_tests.py --check logs/
```

The Easy-FS writable-path tests (`open`, `easyfs_*`) automatically select `ROOTFS=easyfs` and `FS_LIST="easyfs devtmpfs"`. The `backend` test runs on `ROOTFS=ext4` with devtmpfs to confirm capability separation.

Use `python3 ./scripts/run_tests.py --list` for the current automated test set. Manual/demo entries such as `fvsh`, `init`, and `echo` are intentionally hidden from that list.

## Philosophy

* **Elegant Simplicity**: Small code, clear shape, real behavior.
* **Real Boundaries**: Keep the kernel compact while preserving true OS structure.
* **Working System First**: Make paths boot, run, read, write, and fail visibly.
* **Purposeful Abstraction**: Abstract only when it makes the system simpler to grow.
* **Classic Roots, Own Path**: Learn from xv6, but let FrostVista become its own kernel.

---

## Acknowledgments

In its early development stages, FrostVista OS drew significant inspiration from the **xv6** operating system developed by MIT.  
We thank the xv6 authors for their clear, educational implementation of Unix‑like kernel concepts, which laid the foundation for our understanding of file systems, process management, and device drivers.  
The xv6 source code and accompanying textbook (https://pdos.csail.mit.edu/6.828/2023/xv6.html) served as a primary reference throughout the initial design and implementation of FrostVista.
