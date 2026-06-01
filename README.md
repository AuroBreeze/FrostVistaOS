# FrostVista OS / 霜见内核

```text
[INFO]     ______                __ _    ___      __       
[INFO]    / ____/________  _____/ /| |  / (_)____/ /_____ _
[INFO]   / /_  / ___/ __ \/ ___/ __/ | / / / ___/ __/ __ `/
[INFO]  / __/ / /  / /_/ (__  ) /_ | |/ / (__  ) /_/ /_/ / 
[INFO] /_/   /_/   \____/____/\__/ |___/_/____/\__/\__,_/
[INFO] FrostVistaOS booting...
[INFO] Enable time interrupts...
[INFO] Timer init done
[INFO] Paging enable successfully
[INFO] kalloc_init start
[INFO] Total Memory Pages: 32621
[INFO] kalloc_init end
[INFO] clear low memory mappings
[INFO] clear low memory mappings done
[INFO] Hello FrostVista OS!
```

FrostVista is a compact **RISC-V 64 (Sv39)** kernel shaped by a simple idea:
keep the system small, but let every boundary be real.

It is built for learning, experimentation, and small embedded-style
environments. The kernel favors clear structure, direct code, and working
system paths over broad compatibility or unnecessary abstraction.

---

## Roadmap (v0.8 - Pipe & Unix IPC Milestone)

v0.8 focuses on the first real Unix-style inter-process communication path. The milestone is intentionally centered on anonymous pipes, but the real target is the file descriptor, file object, blocking I/O, and process lifecycle behavior that pipes require.

This milestone does not aim to add a shell pipeline parser, sockets, named FIFOs, `poll`/`select`, EXT4 write support, or broad mount work. It should make simple parent-child pipe communication reliable enough that later shell and IPC work can build on it.

### Phase 1 - File Object Dispatch
 - [ ] **Add pipe-backed file objects**: Extend the file type model so file descriptors can refer to either VFS nodes or pipe endpoints.
 - [ ] **Make file operations type-aware**: Route read, write, and close through the correct backend without assuming every descriptor owns a VFS inode.
 - [ ] **Preserve existing VFS behavior**: Keep regular files and devtmpfs devices working through the same file descriptor paths after pipe support is added.

### Phase 2 - Pipe Buffer and Blocking Semantics
 - [ ] **Implement anonymous pipe state**: Add a bounded in-kernel ring buffer with readable and writable endpoint state.
 - [ ] **Support blocking reads and writes**: Use the scheduler sleep/wakeup path when readers wait for data or writers wait for space.
 - [ ] **Handle EOF and broken-pipe cases**: Return EOF when writers are gone and fail writes when readers are gone.

### Phase 3 - `pipe2` Syscall Integration
 - [ ] **Decode `pipe2` arguments**: Add the syscall entry and copy the two allocated file descriptors back to userspace.
 - [ ] **Allocate endpoint descriptors safely**: Create two pipe-backed file objects with correct readable/writable permissions.
 - [ ] **Harden failure rollback**: Release partially allocated pipe, file table, and fd state on allocation or copyout failure.

### Phase 4 - Process and Descriptor Lifecycle
 - [ ] **Verify fork inheritance**: Ensure child processes inherit pipe file descriptors with correct reference counts.
 - [ ] **Verify close and dup behavior**: Keep pipe endpoints alive while duplicated descriptors exist and wake peers when the final endpoint closes.
 - [ ] **Preserve wait/exit behavior**: Ensure process exit closes pipe descriptors and unblocks waiting pipe readers or writers.

### Phase 5 - Pipe Regression Tests
 - [ ] **Basic pipe transfer**: Test one-process write/read behavior through a pipe.
 - [ ] **Fork pipe communication**: Test child-to-parent and parent-to-child communication across `fork`.
 - [ ] **Endpoint lifecycle tests**: Test EOF after closing writers, write failure after closing readers, and dup-based lifetime extension.
 - [ ] **Large transfer test**: Exercise transfers larger than the pipe buffer to verify blocking and wakeup behavior.

---

## Roadmap (v0.7 - Filesystem Architecture & Device Model Milestone)

v0.7 turns the contest-oriented filesystem path from v0.6 into a cleaner multi-filesystem foundation. The milestone focuses on separating generic VFS behavior from filesystem-specific implementation details, introducing a real device filesystem, and retiring the temporary mock `/dev/tty` path left from earlier milestones.

This milestone is primarily architectural. It does not aim to complete full EXT4 write support or broad POSIX mount semantics. Instead, it establishes the boundaries that future filesystem and device work can build on safely.

### Phase 1 - VFS Boundary Cleanup
 - [x] **Clarify generic filesystem responsibilities**: Keep path traversal, file descriptor dispatch, common inode/file abstractions, and mount-point handling in the VFS layer.
 - [x] **Move backend-specific behavior behind filesystem operations**: Ensure Easy-FS, EXT4, and future filesystems provide their behavior through VFS-facing operation tables instead of leaking layout details into generic code.
 - [x] **Remove contest-era shortcuts**: Replace temporary EXT4/Easy-FS branches in generic paths with normal backend dispatch.

### Phase 2 - Filesystem Backend Separation
 - [x] **Make Easy-FS a self-contained backend**: Keep Easy-FS allocation, inode persistence, directory handling, and file data mapping inside the Easy-FS implementation.
 - [x] **Make EXT4 a formal read-only backend**: Preserve the v0.6 contest reader while exposing it through the same VFS-facing model as other filesystems.
 - [x] **Keep shared infrastructure generic**: Restrict common block and inode cache code to filesystem-independent caching, locking, and lifecycle responsibilities.

### Phase 3 - devtmpfs and Device Files
 - [x] **Introduce devtmpfs**: Add an in-memory filesystem for kernel-created device nodes.
 - [x] **Move `/dev/tty` out of the mock VFS tree**: Represent the console as a real device file reachable through normal pathname lookup.
 - [x] **Unify device I/O with file I/O**: Route console read/write through the same file operation path used by regular files.

### Phase 4 - Mount and Boot Integration
 - [x] **Separate root filesystem and device filesystem**: Allow the boot rootfs and `/dev` to come from different filesystem backends.
 - [x] **Preserve existing boot paths**: Keep the Easy-FS fallback, OpenSBI boot path, and EXT4 contest runner working while the architecture is cleaned up.
 - [x] **Prepare for future mount support**: Establish enough internal mount structure to support later user-visible mount and umount work.

### Phase 5 - Validation and Documentation
 - [x] **Regression test core boot flows**: Verify local Easy-FS boot, OpenSBI boot, and EXT4 contest runner behavior after the split.
 - [x] **Regression test device I/O**: Verify stdio and `/dev/tty` behavior through devtmpfs.
 - [x] **Document the new boundaries**: Update roadmap and architecture notes so future filesystem work follows the new VFS/backend split.

---


## Memory Layout

FrostVista utilizes the Sv39 virtual addressing scheme:

```text
0xFFFFFFC080000000  ->  Kernel Base (Virtual)
        |                   maps to
0x0000000080000000  ->  Physical RAM Start
```

Under OpenSBI, the kernel is loaded at `0x80200000`, but QEMU virt RAM
still starts at `0x80000000` and spans 128 MiB. The kernel therefore keeps
the physical memory limit based on the DRAM base:

```text
DRAM_BASE_LOW   = 0x80000000
KERNEL_BASE_LOW = 0x80200000  // OpenSBI boot
PHYSTOP_LOW     = DRAM_BASE_LOW + 128 MiB = 0x88000000
```

This avoids treating the OpenSBI kernel entry offset as extra RAM.

## Build & Run

**Requirements:**

* `riscv64-elf-gcc` (or similar cross-compiler)
* `qemu-system-riscv64`
* `make`

**To build and launch QEMU with the default local configuration:**

```bash
make qemu
```

You should see the kernel enabling paging and jumping to the higher half address space in the serial console. The `qemu` target respects explicit build parameters, so use it as the normal hand-written run entry point.

Useful parameters:

```text
BOOT=bare|opensbi
ROOTFS=easyfs|ext4
FS_LIST="easyfs devtmpfs"|"ext4 devtmpfs"
TEST=<test name under test/test_*.c, without the test_ prefix>
BUILD=release|debug
```

For the OpenSBI EXT4 runner path:

```bash
make qemu BOOT=opensbi ROOTFS=ext4 FS_LIST="ext4 devtmpfs" TEST=runner BUILD=debug
```

For a paused GDB session on the same path:

```bash
make debug BOOT=opensbi ROOTFS=ext4 FS_LIST="ext4 devtmpfs" TEST=runner
make gdb
```

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
