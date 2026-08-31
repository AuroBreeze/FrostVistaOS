# FrostVista OS / 霜见内核

A compact teaching kernel with real process, filesystem, and device paths.

Sample RISC-V boot log:

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

FrostVista is shaped by a simple idea: keep the system small, but let every
boundary be real. It is built for learning, experimentation, and small
embedded-style environments. The kernel favors clear structure, direct code,
and working system paths over broad compatibility or unnecessary abstraction.

## Current Status

| Target | Status | Boot and runtime |
| --- | --- | --- |
| RISC-V 64 (Sv39) | Primary target | Bare-metal or OpenSBI, Easy-FS/EXT4/tmpfs |
| LoongArch64 | Experimental bring-up | Direct QEMU boot, tmpfs/devtmpfs |

The current release is **v1.5.0 — LoongArch64 Bring-Up**. LoongArch64 can
boot directly on QEMU and run embedded user programs, but it does not yet
have feature parity with RISC-V.

---

## Project Layout

```text
FrostVistaOS/
|-- arch/
|   |-- riscv/              RISC-V boot, trap, paging, SBI, UART, timer, and PLIC code
|   |   |-- boot/
|   |   |-- driver/
|   |   |-- include/
|   |   |-- mm/
|   |   |-- tool/
|   |   `-- trap/
|   `-- loongarch/          LoongArch64 direct boot, traps, paging, UART, and timer code
|-- kernel/
|   |-- core/               Process, syscall, exec, file descriptor, pipe, and scheduler paths
|   |-- driver/             VirtIO block device driver
|   |-- fs/                 VFS, Easy-FS, EXT4 read-only, devtmpfs, tmpfs, and block cache layers
|   |   |-- devtmpfs/
|   |   |-- easyfs/
|   |   |-- ext4fs/
|   |   `-- tmpfs/
|   `-- mm/                 Kernel memory management
|-- include/                Kernel headers and shared constants
|-- mk/                     Makefile fragments for toolchain, sources, images, run profiles, and checks
|-- mkfs/                   Host Easy-FS image builder
|-- scripts/                Test runner and helper scripts
|-- test/                   User-mode test entry programs; each test/test_*.c can become /init
|-- user/                   Shared user-mode runtime
|   `-- bin/                User applications packaged into Easy-FS, such as echo, cat, and fvsh
|-- docs/                   Project notes and known issues
`-- devlog/                 Development notes
```

The test/application split is intentional:

```text
test/test_$(TEST).c  -> build/test/init_bin -> guest /init
user/bin/*.c         -> build/user/<app>    -> guest /<app>
```

`test/` programs are test entrypoints. `user/bin/` programs are normal user applications placed in the Easy-FS image. The shared user runtime lives in `user/user.h` and `user/ulib.c`.

---

## Roadmap

See [`releases.md`](./releases.md) for the active roadmap, milestone history,
validation commands, and known follow-up work.

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

| Limitation | Example |
| --- | --- |
| No quotes or escapes | `echo "hello world"` |
| No append redirection | `echo hi >> out` |
| No stderr redirection | `cmd 2> err` |
| No multi-stage pipelines | `a → b → c` |
| No globbing or variables | `echo $HOME`, `ls *.c` |
| No job control or backgrounding | `cmd &`, `fg`, `bg` |
| No `PATH`/environment search model | Applications are packaged in Easy-FS |

## Build & Run

### Requirements

Install the host tools plus the cross-compiler and emulator for the target
you want to run:

| Target | Cross-compiler | Emulator |
| --- | --- | --- |
| RISC-V 64 | `riscv64-elf-gcc` or a compatible toolchain | `qemu-system-riscv64` |
| LoongArch64 | `loongarch64-unknown-linux-gnu-gcc` or `loongarch64-elf-gcc` | `qemu-system-loongarch64` |

`make` is required for both targets.

### Common Build Parameters

| Parameter | Values | Purpose |
| --- | --- | --- |
| `ARCH` | `riscv`, `loongarch` | Select the target architecture |
| `BOOT` | `bare`, `opensbi` | Select the boot path where supported |
| `ROOTFS` | `easyfs`, `ext4`, `tmpfs` | Select the root filesystem |
| `FS_LIST` | Space-separated filesystem names | Enable filesystem layers |
| `TEST` | Test name without `test_` | Select the `/init` test program |
| `BUILD` | `release`, `debug` | Select optimization and debug info |

### RISC-V 64

Build and launch QEMU with the default interactive shell configuration:

```bash
make qemu ROOTFS=easyfs FS_LIST="devtmpfs tmpfs" TEST=fvsh
```

You should see the kernel enabling paging, mounting Easy-FS/devtmpfs, and starting the FrostVista shell (`fvsh`) in the serial console. The `qemu` target respects explicit build parameters, so use it as the normal hand-written run entry point.

The Easy-FS image is built by `mkfs/mkfs.c`. For shell runs it contains `/init`
from the selected test plus packaged applications from `user/bin/`, currently
including `/echo`, `/cat`, and `/fvsh`.

Manual/demo tests such as `fvsh`, `init`, and `echo` are not part of the automated test list. Use `fvsh_script` for automated shell regression.

For the OpenSBI EXT4 runner path:

```bash
make qemu BOOT=opensbi ROOTFS=ext4 FS_LIST="devtmpfs tmpfs" TEST=fvsh
```

For a paused GDB session on the same path:

```bash
make debug BOOT=opensbi ROOTFS=ext4 FS_LIST="devtmpfs tmpfs" TEST=fvsh
make gdb
```

### LoongArch64 Bring-Up

The LoongArch64 target uses direct bare-metal boot on QEMU. The build selects
`loongarch64-unknown-linux-gnu` when available and falls back to
`loongarch64-elf`; set `CROSS` explicitly to use another compatible toolchain.

Run the shell and the first user-program smoke test with:

```bash
make ARCH=loongarch TEST=fvsh qemu
make ARCH=loongarch TEST=argc qemu
```

LoongArch runs without a block-device image and uses `tmpfs` plus `devtmpfs`.
Validate the direct-boot image without launching QEMU interactively:

```bash
make ARCH=loongarch ROOTFS=tmpfs FS_LIST="tmpfs devtmpfs" \
  check-direct-boot
```

The port is still under active bring-up. The initial automated test set is
architecture-aware, but some memory-management, process, and signal tests
remain incomplete; see [`releases.md`](./releases.md) for the current
validation record.

## Automated Tests

The Python runner builds one user test at a time, launches QEMU, records logs
under `logs/`, and classifies kernel diagnostics. Expected diagnostics from
negative syscall tests are reported as `PASS_EXPECTED_LOG`; unexpected
`[WARN]` or `[ERROR]` lines are surfaced separately. `sys_pipe` also
count-limits its expected diagnostics so extra matching warnings are not
silently accepted.

```bash
python3 ./scripts/run_tests.py --list
python3 ./scripts/run_tests.py -t fvsh_script -T 30
python3 ./scripts/run_tests.py -t sys_pipe -T 20 --skip-kernel
python3 ./scripts/run_tests.py -t easyfs -T 20 --skip-kernel
python3 ./scripts/run_tests.py -t backend -T 20 --skip-kernel --rootfs ext4 --fs-list "tmpfs devtmpfs"
python3 ./scripts/run_tests.py --check logs/
```

The Easy-FS writable-path tests (`open`, `easyfs_*`) automatically select
`ROOTFS=easyfs` and `FS_LIST="easyfs devtmpfs"`. The `backend` test runs on
`ROOTFS=ext4` with tmpfs to confirm capability separation under the overlay:
the read-only EXT4 image stays unchanged while writes land in the tmpfs upper
layer.

Use `python3 ./scripts/run_tests.py --list` for the current automated test set. Manual/demo entries such as `fvsh`, `init`, and `echo` are intentionally hidden from that list.

## Philosophy

- **Elegant Simplicity**: Small code, clear shape, real behavior.
- **Real Boundaries**: Keep the kernel compact while preserving true OS structure.
- **Working System First**: Make paths boot, run, read, write, and fail visibly.
- **Purposeful Abstraction**: Abstract only when it makes the system simpler to grow.
- **Classic Roots, Own Path**: Learn from xv6, but let FrostVista become its own kernel.

---

## Acknowledgments

In its early development stages, FrostVista OS drew significant inspiration
from the **xv6** operating system developed by MIT. We thank the xv6 authors
for their clear, educational implementation of Unix-like kernel concepts,
which laid the foundation for our understanding of filesystems, process
management, and device drivers. The [xv6 source code and textbook](https://pdos.csail.mit.edu/6.828/2023/xv6.html)
served as a primary reference throughout FrostVista's initial design and
implementation.

---

## Community

Join our Discord server to discuss development, ask questions, and share ideas:

[https://discord.gg/N8Ar3q5cSh](https://discord.gg/N8Ar3q5cSh)
