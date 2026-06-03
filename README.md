# FrostVista OS / 霜见内核

```text
[   0.094] [ INFO] Paging enable successfully
------------------------------------------------------------
    ______                __ _    ___      __       
   / ____/________  _____/ /| |  / (_)____/ /_____ _
  / /_  / ___/ __ \/ ___/ __/ | / / / ___/ __/ __ `/
 / __/ / /  / /_/ (__  ) /_ | |/ / (__  ) /_/ /_/ / 
/_/   /_/   \____/____/\__/ |___/_/____/\__/\__,_/

RISC-V 64  |  Sv39  |  v0.7
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

## Current Milestone (v0.9 - Easy-FS Completion & Writable VFS)

v0.9 focuses on making FrostVista's local filesystem path useful as a real writable Unix-style storage layer. v0.8 completed the anonymous pipe IPC primitive; v0.9 moves back down to persistent files so later shell, redirection, and user workflow milestones have a stable filesystem to stand on.

This milestone does not aim to make EXT4 writable, add a full POSIX mount model, or implement shell pipelines. EXT4 remains the read-only contest/root image path. Easy-FS is the writable local backend that should support regular file creation, overwrite, append, truncation, directory updates, and persistence-oriented regression tests.

### Phase 1 - VFS Write Contract
 - [x] **Define basic open flag behavior**: `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, and invalid `O_TRUNC | O_RDONLY` handling are covered in the VFS path.
 - [ ] **Clarify file offset rules**: Keep `read`, `write`, and `lseek` offset behavior consistent across regular files, devices, and pipes.
 - [ ] **Separate backend capabilities**: Let Easy-FS expose writable regular files while EXT4 stays read-only and devtmpfs stays device-oriented.

### Phase 2 - Easy-FS File Writes
 - [x] **Create regular files**: Support creating a missing regular file through the VFS/open path with `O_CREAT`.
 - [x] **Write file data**: Persist direct-block writes to Easy-FS regular files and preserve correct file size updates.
 - [x] **Support append and truncation**: Implement append-at-end and truncate-to-empty behavior for regular files.
 - [x] **Handle cross-block writes**: Exercise writes that span multiple Easy-FS blocks without corrupting neighboring files.

### Phase 3 - Directory and Path Operations
 - [ ] **Allocate directory entries safely**: Add, reuse, and validate Easy-FS directory entries without leaking stale names.
 - [x] **Support unlink basics**: Remove regular files and release their inode/data blocks when safe.
 - [ ] **Support mkdir basics**: Create directories with correct parent/child path lookup behavior.
 - [ ] **Harden path edges**: Cover empty paths, missing parents, duplicate creates, and path length boundaries.

### Phase 4 - Persistence Regression Tests
 - [x] **Reopen-after-close tests**: Write a file, close it, reopen it, and verify the data and size.
 - [x] **Multi-file allocation tests**: Create and write several files to ensure block allocation does not overlap.
 - [x] **Truncate and append tests**: Verify data after truncation, append, and overwrite sequences.
 - [x] **Unlink tests**: Confirm removed files cannot be reopened and remaining files still read correctly.

### Phase 5 - Userland FS Coverage
 - [x] **Add focused Easy-FS tests**: `test_open` covers open flags, create/write/read persistence, truncate, append, multi-file allocation, cross-block writes, and cross-block append; `test_easyfs_maxfile` covers the current 12-direct-block file limit; `test_easyfs_unlink` covers regular-file deletion and post-unlink allocation.
 - [x] **Update the Python runner**: Include `open` and `easyfs_maxfile` in the automated test list and run them with `ROOTFS=easyfs FS_LIST="easyfs devtmpfs"`.
 - [ ] **Preserve existing paths**: Keep `sys_pipe`, `io`, `vfs`, EXT4 read-only boot, and devtmpfs regressions passing while Easy-FS write support is completed.

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

## Automated Tests

The Python runner builds one user test at a time, launches QEMU, records logs under `logs/`, and classifies kernel diagnostics. Expected diagnostics from negative syscall tests are reported as `PASS_EXPECTED_LOG`; unexpected `[WARN]` or `[ERROR]` lines are surfaced separately. `sys_pipe` also count-limits its expected diagnostics so extra matching warnings are not silently accepted.

```bash
python3 ./scripts/run_tests.py --list
python3 ./scripts/run_tests.py -t sys_pipe -T 20 --skip-kernel
python3 ./scripts/run_tests.py -t open -T 20 --skip-kernel
python3 ./scripts/run_tests.py -t easyfs_maxfile -T 20 --skip-kernel
python3 ./scripts/run_tests.py -t easyfs_unlink -T 20 --skip-kernel
python3 ./scripts/run_tests.py --check logs/
```

The `open`, `easyfs_maxfile`, and `easyfs_unlink` regressions are Easy-FS writable-path tests. The runner selects `ROOTFS=easyfs` and `FS_LIST="easyfs devtmpfs"` automatically for them.

Current focused regression tests include:

```text
sbrk fork sys_write sys_misc sys_pipe open easyfs_maxfile easyfs_unlink io vfs lazy_copy runner
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
