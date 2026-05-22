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

FrostVista is a lightweight, educational operating system kernel targeting **RISC-V 64 (Sv39)**.

---

# Roadmap (v0.5 - The Cleanup & Consolidation Milestone)

With the file system operational, v0.5 tears down the development scaffolding erected during v0.4 and unifies the codebase under a single, consistent architecture. No new features — this is a pure quality milestone.

The critical architectural debt: `open()` still resolves paths through a mock VFS tree (`vfs_lookup` -> `mock_finddir`) so `/dev/tty` can exist before a real device filesystem is available, while `create()`/`unlink()` use the real Easy-FS (`namei`/`nameiparent`). This split cannot be fully removed until v0.6 introduces devtmpfs.

## v0.6 Preview - devtmpfs
 - [ ] **Introduce devtmpfs for device nodes**: Move `/dev/tty` and future device entries into a dedicated device filesystem. This will replace the current hardwired UART handling and allow the remaining mock VFS device scaffolding to be deleted cleanly.

## Phase 1 - VFS Debt Tracking
 - [ ] **Defer `open()` path unification until devtmpfs**: Keep the current `/dev/tty` mock path alive until v0.6 can provide device nodes through devtmpfs.
 - [ ] **Keep mock VFS infrastructure scoped**: `mock_finddir`, `default_mock_ops`, `create_vfs_inode`, and the `dev_dir`/`tty_file` globals remain temporary compatibility code, not the target VFS design.
 - [ ] **Remove `test_vfs()` when devtmpfs lands**: The test only exercises the mock tree, so delete it together with the mock device path.
 - [ ] **Drop commented-out O_CREATE block**: The stale draft block in `open()` (`file.c:16-33`) can still be cleaned independently because it is not part of the current `/dev/tty` compatibility path.

## Phase 2 - Magic Number Elimination
 - [x] **FS layout constants**: Define `EASYFS_INODE_BITMAP_BLOCK`, `EASYFS_INODE_BLOCK`, `EASYFS_DATA_START` for block numbers hardcoded as `2`, `3`, `4` in `ialloc()`, `balloc()`, and mount code.
 - [x] **Path buffer**: Replace repeated `128`/`127` with `PATH_MAX` in `vfs.c`, `fs.c`, `sysfile.c`.
 - [x] **Syscall argument offsets**: Replace `argint(0,...)`, `argaddr(1,...)` offset literals with named constants in `syscall.c`.
 - [x] **Printf constants**: Name the `32`, `16`, `60` format buffer sizes in `printf.c`.

## Phase 3 - Code Quality Fixes
 - [x] **Fix typos and spelling errors** across the codebase (struct fields, comments, log messages).
 - [x] **Normalize log levels**: Audit every `LOG_*` call site — errors should use `LOG_ERROR`, warnings `LOG_WARN`, normal operations `LOG_DEBUG` or `LOG_TRACE`. Fix inconsistencies like `sys_write` logging a permissions failure at TRACE while the file-not-found case uses ERROR.
 - [x] **Remove dead declarations** from header files (commented-out structs, unused function prototypes).
 - [x] **Return value convention**: Fix error paths returning 0 instead of -1 in sys_write, sys_read, sys_exec, exec, and loadseg.
 - [x] **Fix inode private_data lifecycle**: Move allocation to `get_inode` (cold slot reuse) and deallocation to `put_inode` (eviction), remove alloc/free from `ilock`/`iunlock`. Eliminate `kalloc` leaks in `exec.c` (stack-allocate ELF headers) and `mount.c` (free directory entry buffer).
 - [x] **Move string helpers into core**: Relocate common string/memory routines from `kernel/mm` to `kernel/core`, matching their cross-subsystem role.
 - [x] **Harden `exec()` cleanup**: Ensure `ilock()` error paths release the inode, check `loadseg()` failures, and free the old address space without reporting a successful exec as an error.
 - [x] **Fix Easy-FS inode cleanup hazards**: Correct `itrunc()` so it frees only allocated direct blocks, and avoid copying whole cached inode structures during pathname traversal.
 - [x] **Harden syscall and file I/O edges**: Release `ftable_lock` on `open()` allocation failure, guard syscall logging against invalid numbers, validate file ops before read/write, and normalize negative device I/O returns to `-1`.
 - [x] **Remove VFS declaration warning**: Move `struct vfs_dirent` before `struct vfs_file_ops` so the kernel build no longer emits the forward declaration warning.
 - [x] **Lock documentation**: Document inode reference ownership, inode sleeplock acquire/release contracts, and buffer cache lock ownership for the core Easy-FS and bcache helpers.

---


## Memory Layout

FrostVista utilizes the Sv39 virtual addressing scheme:

```text
0xFFFFFFC080000000  ->  Kernel Base (Virtual)
        |                   maps to
0x0000000080000000  ->  Physical RAM Start
```

## Build & Run

**Requirements:**

* `riscv64-elf-gcc` (or similar cross-compiler)
* `qemu-system-riscv64`
* `make`

**To build and launch QEMU:**

```bash
make run
```

You should see the kernel enabling paging and jumping to the higher half address space in the serial console.

## Philosophy

* **Clarity over Cleverness**: Code is written to be understood.
* **Architecture First**: Implementing proper OS concepts (Virtual Memory, Traps) rather than hacking features.
* **From Scratch**: Minimizing external dependencies to understand the hardware.

---

## Acknowledgments

In its early development stages, FrostVista OS drew significant inspiration from the **xv6** operating system developed by MIT.  
We thank the xv6 authors for their clear, educational implementation of Unix‑like kernel concepts, which laid the foundation for our understanding of file systems, process management, and device drivers.  
The xv6 source code and accompanying textbook (https://pdos.csail.mit.edu/6.828/2023/xv6.html) served as a primary reference throughout the initial design and implementation of FrostVista.
