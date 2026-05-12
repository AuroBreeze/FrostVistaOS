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

The critical architectural debt: `open()` resolves paths through a mock VFS tree (`vfs_lookup` → `mock_finddir`), while `create()`/`unlink()` use the real Easy-FS (`namei`/`nameiparent`). These two code paths have never been connected. The mock tree was a development scaffold that must be removed now that the real FS works.

## Phase 1 - Unify VFS Path Resolution
 - [ ] **Route `open()` through `namei`**: Replace `vfs_lookup(vfs_root, ...)` in `open()` with the real `namei()` path resolution that `create()` already uses, so both share a single source of truth.
 - [ ] **Remove mock VFS infrastructure**: Delete `mock_finddir`, `default_mock_ops`, `create_vfs_inode`, and the `dev_dir`/`tty_file` mock globals from `vfs.c`.
 - [ ] **Remove `test_vfs()`**: Obsolete test function that only exercised the mock tree.
 - [ ] **Drop commented-out O_CREATE block**: The 15-line block in `open()` (`file.c:16-33`) was a draft of the real resolution — delete it now that `namei` handles this.

## Phase 2 - UART Console Consolidation
 - [ ] **Extract UART file ops to `kernel/dev/console.c`**: Move `uart_vfs_write`, `uart_vfs_read`, and the `uart_ops` struct out of `vfs.c` into a dedicated device file, removing the VFS's dependency on `hal_console.h`.
 - [ ] **Wire device ops through inode type**: When `open()` encounters a `VFS_DEV` inode, attach device-specific file ops rather than defaulting every inode to `uart_ops`.

## Phase 3 - Magic Number Elimination
 - [x] **FS layout constants**: Define `EASYFS_INODE_BITMAP_BLOCK`, `EASYFS_INODE_BLOCK`, `EASYFS_DATA_START` for block numbers hardcoded as `2`, `3`, `4` in `ialloc()`, `balloc()`, and mount code.
 - [x] **Path buffer**: Replace repeated `128`/`127` with `PATH_MAX` in `vfs.c`, `fs.c`, `sysfile.c`.
 - [ ] **Syscall argument offsets**: Replace `argint(0,...)`, `argaddr(1,...)` offset literals with named constants in `syscall.c`.
 - [ ] **Printf constants**: Name the `32`, `16`, `60` format buffer sizes in `printf.c`.

## Phase 4 - Code Quality Fixes
 - [ ] **Fix typos and spelling errors** across the codebase (struct fields, comments, log messages).
 - [ ] **Normalize log levels**: Audit every `LOG_*` call site — errors should use `LOG_ERROR`, warnings `LOG_WARN`, normal operations `LOG_DEBUG` or `LOG_TRACE`. Fix inconsistencies like `sys_write` logging a permissions failure at TRACE while the file-not-found case uses ERROR.
 - [ ] **Remove dead declarations** from header files (commented-out structs, unused function prototypes).
 - [ ] **Lock documentation**: Document the sleeplock acquire/release contract for each inode function — what lock is held on entry, who releases it, and why certain functions expect the caller to release.

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
