# FrostVista OS / 霜见内核

[![RISC-V regression tests](https://github.com/AuroBreeze/FrostVistaOS/actions/workflows/riscv-tests.yml/badge.svg)](https://github.com/AuroBreeze/FrostVistaOS/actions/workflows/riscv-tests.yml) [![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](./LICENSE)

FrostVistaOS is a compact teaching kernel for RISC-V 64, with an experimental LoongArch64 port. It implements real process, virtual-memory, filesystem, and device paths while keeping the codebase small enough to study and modify.

The current release is **v1.5.0 — LoongArch64 Bring-Up**. RISC-V 64 is the primary target. LoongArch64 can boot directly on QEMU and run embedded user programs, but it does not yet have feature parity with RISC-V.

## Features

- User processes, scheduling, `fork`, `exec`, and `wait`
- System calls, file descriptors, pipes, and signals
- Virtual memory, anonymous `mmap`, lazy allocation, and copy-on-write
- VFS with EasyFS, read-only EXT4, tmpfs, and devtmpfs backends
- VirtIO block devices on RISC-V
- A small interactive shell, `fvsh`
- Architecture-aware QEMU regression testing

Some of these paths are still incomplete on LoongArch64. The support matrix below states the current boundary explicitly.

## Platform Support

| Capability | RISC-V 64 | LoongArch64 |
| --- | --- | --- |
| QEMU `virt` boot | Supported | Supported |
| Boot mode | Bare metal or OpenSBI | Direct bare metal |
| User mode and system calls | Supported | Bring-up complete |
| Process and memory regressions | Supported | In progress |
| Page faults, COW, and `mmap` | Supported | In progress |
| tmpfs and devtmpfs | Supported | Supported |
| VirtIO block and EasyFS | Supported | Not yet supported |
| EXT4 | Read-only | Not yet supported |
| Automated CI | Enabled | Not yet enabled |

See [`releases.md`](./releases.md) for the active roadmap and validation record, and [`CHANGELOG.md`](./CHANGELOG.md) for release notes.

## Quick Start

### Requirements

Both targets require `make`, a matching cross-compiler, and QEMU:

| Target | Cross-compiler | Emulator |
| --- | --- | --- |
| RISC-V 64 | `riscv64-elf-gcc` or a compatible toolchain | `qemu-system-riscv64` |
| LoongArch64 | `loongarch64-unknown-linux-gnu-gcc` or `loongarch64-elf-gcc` | `qemu-system-loongarch64` |

### RISC-V 64

Build the kernel, create an EasyFS image, and start `fvsh`:

```bash
make qemu ROOTFS=easyfs FS_LIST="devtmpfs tmpfs" TEST=fvsh
```

The build places the selected `test/test_*.c` program at `/init`. For the shell configuration, the EasyFS image also contains applications from `user/bin/`, including `/echo`, `/cat`, and `/fvsh`.

An abbreviated boot log looks like this:

```text
[ INFO] Paging enable successfully
[ INFO] Timer init done
[ INFO] Total Memory Pages: 32039
[ INFO] virtio-blk initialized, mmio version 2
[ INFO] Hello FrostVista OS!
```

### LoongArch64

LoongArch64 currently runs without a block-device image. Start the shell or the initial user-program smoke test with tmpfs and devtmpfs:

```bash
make ARCH=loongarch ROOTFS=tmpfs FS_LIST="devtmpfs" TEST=fvsh qemu
make ARCH=loongarch ROOTFS=tmpfs FS_LIST="devtmpfs" TEST=argc qemu
```

Validate the direct-boot image without starting QEMU interactively:

```bash
make ARCH=loongarch ROOTFS=tmpfs FS_LIST="devtmpfs" check-direct-boot
```

The build selects `loongarch64-unknown-linux-gnu` when available and falls back to `loongarch64-elf`. Set `CROSS` explicitly to use another compatible toolchain.

## Build Configuration

The main build variables are:

| Variable | Values | Purpose |
| --- | --- | --- |
| `ARCH` | `riscv`, `loongarch` | Select the target architecture |
| `BOOT` | `bare`, `opensbi` | Select the boot path where supported |
| `ROOTFS` | `easyfs`, `ext4`, `tmpfs` | Select the root filesystem |
| `FS_LIST` | Space-separated filesystem names | Enable additional filesystem backends |
| `TEST` | Test name without `test_` | Select the program installed as `/init` |
| `BUILD` | `release`, `debug` | Select optimization or debug information |

`ROOTFS` is automatically included in `FS_LIST`.

Run RISC-V with the OpenSBI and EXT4 configuration:

```bash
make qemu BOOT=opensbi ROOTFS=ext4 FS_LIST="devtmpfs tmpfs" TEST=fvsh
```

Start a debug build paused for GDB, then connect from a second terminal:

```bash
make debug BOOT=opensbi ROOTFS=ext4 FS_LIST="devtmpfs tmpfs" TEST=fvsh
make gdb
```

## Testing

The test runner builds one user test at a time, launches QEMU, records output under `logs/`, and checks both test results and kernel diagnostics.

```bash
python3 scripts/run_tests.py --list
python3 scripts/run_tests.py -t fvsh_script -T 30
python3 scripts/run_tests.py -t sys_pipe -T 20 --skip-kernel
python3 scripts/run_tests.py --check logs/
```

Run the current LoongArch64 diskless test set with:

```bash
python3 scripts/run_tests.py --arch loongarch --rootfs tmpfs \
  --fs-list "tmpfs devtmpfs" -T 20
```

Manual programs such as `fvsh`, `init`, and `echo` are intentionally omitted from `--list`; use `fvsh_script` for automated shell regression. Tests that exercise writable EasyFS paths select the required filesystem configuration automatically.

## FrostVista Shell

`fvsh` is a small shell for exercising process, file-descriptor, EasyFS, and pipe paths. It supports built-ins, external commands, single pipelines, and basic input/output redirection:

```text
help
pwd
cd /
echo hello
cat file
echo hello > out
cat < out
echo hello | cat > out
exit
```

It is not intended to be a complete POSIX shell.

| Current limitation | Example |
| --- | --- |
| No quoting or escaping | `echo "hello world"` |
| No append or standard-error redirection | `echo hi >> out`, `cmd 2> err` |
| No pipelines with more than two commands | `a \| b \| c` |
| No globbing or variables | `ls *.c`, `echo $HOME` |
| No job control or background execution | `cmd &`, `fg`, `bg` |
| No `PATH` or environment search model | Applications are packaged in EasyFS |

## Project Layout

```text
FrostVistaOS/
|-- arch/                 Architecture-specific boot, trap, paging, and drivers
|   |-- riscv/
|   `-- loongarch/
|-- kernel/
|   |-- core/             Processes, scheduling, syscalls, exec, files, and signals
|   |-- driver/           Shared device drivers
|   |-- fs/               VFS and filesystem backends
|   `-- mm/               Page, heap, and slab allocators
|-- include/              Shared kernel headers
|-- user/                 User runtime and packaged applications
|-- test/                 User-mode test entry programs
|-- mk/                   Shared build rules
|-- mkfs/                 Host-side EasyFS image builder
|-- scripts/              Test and validation tools
|-- docs/                 Development and known-issue documentation
`-- devlog/               Development notes
```

Tests and packaged applications have separate roles:

```text
test/test_$(TEST).c  -> build/<arch>/test/init_bin -> guest /init
user/bin/*.c         -> build/<arch>/user/<app>    -> guest /<app>
```

The shared user runtime lives in `user/user.h` and `user/ulib.c`.

## Project Principles

- **Elegant simplicity:** keep the implementation compact and readable.
- **Real boundaries:** preserve the structure of processes, address spaces, filesystems, and devices.
- **Working paths first:** make features boot, run, read, write, and fail visibly before broadening their scope.
- **Purposeful abstraction:** introduce shared interfaces when they make the kernel easier to extend.
- **Classic roots, independent direction:** learn from xv6 while developing a distinct kernel design.

## Contributing

Contributions and issue reports are welcome. Before changing the repository, read the development guidance under [`docs/development/`](./docs/development/) and follow the existing formatting and test conventions.

## Acknowledgments

FrostVistaOS was initially inspired by MIT's **xv6**. Its clear treatment of processes, virtual memory, filesystems, and device drivers remains an important reference. See the [xv6 source code and textbook](https://pdos.csail.mit.edu/6.828/2023/xv6.html).

## Community

Join the [FrostVistaOS Discord server](https://discord.gg/N8Ar3q5cSh) to discuss development, ask questions, and share ideas.

## License

FrostVistaOS is distributed under the [GNU General Public License v3.0](./LICENSE).
