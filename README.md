# FrostVista OS / 霜见内核

[![RISC-V tests](https://img.shields.io/github/actions/workflow/status/AuroBreeze/FrostVistaOS/riscv-tests.yml?label=RISC-V%20tests&logo=github&style=for-the-badge)](https://github.com/AuroBreeze/FrostVistaOS/actions/workflows/riscv-tests.yml) [![LoongArch tests](https://img.shields.io/github/actions/workflow/status/AuroBreeze/FrostVistaOS/loongarch-tests.yml?label=LoongArch%20tests&logo=github&style=for-the-badge)](https://github.com/AuroBreeze/FrostVistaOS/actions/workflows/loongarch-tests.yml) [![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg?style=for-the-badge)](./LICENSE)

FrostVistaOS is a compact teaching kernel for RISC-V 64, with an experimental LoongArch64 port. It implements real process, virtual-memory, filesystem, and device paths while keeping the codebase small enough to study and modify.

The current release is **v1.6.0 — LoongArch64 Runtime and PCI Storage**. RISC-V 64 remains the primary target. LoongArch64 supports modern VirtIO PCI block I/O, EasyFS root images with user applications, and interrupt-driven UART input/output. Selected runtime regressions pass on both architectures; full feature parity and the complete filesystem test matrix remain in progress.

## Features

- User processes, scheduling, `fork`, `exec`, and `wait`
- System calls, file descriptors, pipes, and signals
- Virtual memory, anonymous `mmap`, lazy allocation, and copy-on-write
- VFS with EasyFS, read-only EXT4, tmpfs, and devtmpfs backends
- VirtIO MMIO block devices on RISC-V and modern VirtIO PCI block devices on LoongArch64
- A small interactive shell, `fvsh`, with packaged user applications and Ctrl+C handling
- Architecture-aware QEMU regression testing

Some of these paths are still incomplete on LoongArch64. The support matrix below states the current boundary explicitly.

## Platform Support

| Capability | RISC-V 64 | LoongArch64 |
| --- | --- | --- |
| QEMU `virt` boot | Supported | Supported |
| Boot mode | Bare metal or OpenSBI | Direct bare metal |
| User mode and system calls | Supported | Supported |
| Process and memory regressions | Supported | Supported |
| Page faults, COW, and `mmap` | Supported | Supported |
| tmpfs and devtmpfs | Supported | Supported |
| VirtIO block and EasyFS | Supported (MMIO) | Supported (modern PCI, INTx) |
| EXT4 | Read-only | Read-only |
| UART RX/TX interrupts and Ctrl+C | Supported | Supported |
| Automated CI | Enabled | Enabled |

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

The build places the selected `test/test_*.c` program at `/init`. EasyFS images also contain applications from `user/bin/`, including `/echo`, `/cat`, and `/fvsh`. Images are separated by architecture under `build/<arch>/easyfs.img`. When the generated init header is present, the kernel loads `/init` from the embedded test image; other application paths are loaded from the mounted filesystem.

An abbreviated boot log looks like this:

```text
[ INFO] Paging enable successfully
[ INFO] Timer init done
[ INFO] Total Memory Pages: 32039
[ INFO] virtio-blk initialized, mmio version 2
[ INFO] Hello FrostVista OS!
```

### LoongArch64

Build an EasyFS image with LoongArch64 applications and start the shell with an EasyFS root mounted through the modern VirtIO PCI block device. A diskless tmpfs configuration remains available for runtime smoke tests:

```bash
make ARCH=loongarch ROOTFS=easyfs FS_LIST="devtmpfs tmpfs" TEST=fvsh qemu
make ARCH=loongarch ROOTFS=tmpfs FS_LIST="devtmpfs" TEST=argc qemu
```

Validate the direct-boot image without starting QEMU interactively:

```bash
make ARCH=loongarch ROOTFS=tmpfs FS_LIST="devtmpfs" check-direct-boot
```

The build selects `loongarch64-unknown-linux-gnu` when available and falls back to `loongarch64-elf`. Set `CROSS` explicitly to use another compatible toolchain.

LoongArch64 defaults to EasyFS and the PCI transport. To generate its image independently, run `make ARCH=loongarch easyfs.img TEST=fvsh`. The interactive startup above uses the embedded shell test; starting `/fvsh` directly from the filesystem is still a release acceptance item.

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
| `CONFIG_VIRTIO_TRANSPORT` | `virtio-mmio`, `virtio-pci` | Select the architecture-supported VirtIO transport |

`ROOTFS` is automatically included in `FS_LIST`. The default transport is `virtio-mmio` on RISC-V and `virtio-pci` on LoongArch64. EXT4 builds select `sdcard-rv.img` or `sdcard-la.img` for the corresponding architecture; provide a matching image when using EXT4.

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

Run LoongArch64 shell, block-interrupt, and application-image regressions, or select the diskless test set:

```bash
python3 scripts/run_tests.py --arch loongarch -t fvsh_script -T 30
python3 scripts/run_tests.py --arch loongarch -t pci_irq -T 30
python3 scripts/run_tests.py --arch loongarch -t user_apps -T 30
python3 scripts/run_tests.py --arch loongarch --rootfs tmpfs \
  --fs-list "tmpfs devtmpfs" -T 30
```

Both architectures expose the same automated test catalog. Explicit `--rootfs` selection filters the catalog to that filesystem; LoongArch64 defaults to EasyFS, while tests with EXT4 requirements select EXT4. Logs include the architecture, test, and boot mode, and timeouts or uncertain results return failure.

Local EasyFS validation covers `getuid`, `set_tid_address`, `fstat`, `signal`, `wait`, `mmap`, and `fvsh_script` on both architectures, plus LoongArch64 PCI block interrupts and interactive UART input. Negative tests may report `PASS_EXPECTED_LOG` for recognized diagnostics.

GitHub Actions runs LoongArch64 tmpfs regressions on pull requests to `main` and `dev`. The complete diskless and EasyFS/EXT4 test matrices remain release acceptance work; the local results above do not establish a full-suite pass.

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

The shared user runtime lives in `user/user.h` and `user/ulib.c`. Architecture syscall helpers live in `arch/<arch>/include/asm/user.h`, and signal-return trampolines live in `arch/<arch>/user/__restore.S`.

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
