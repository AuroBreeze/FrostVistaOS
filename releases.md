## 🎯 TODO

None

---

# 🚀 Roadmap (v0.7 - Filesystem Architecture & Device Model Milestone)

v0.7 turns the contest-oriented filesystem path from v0.6 into a cleaner multi-filesystem foundation. The milestone focuses on separating generic VFS behavior from filesystem-specific implementation details, introducing a real device filesystem, and retiring the temporary mock `/dev/tty` path left from earlier milestones.

This milestone is primarily architectural. It does not aim to complete full EXT4 write support or broad POSIX mount semantics. Instead, it establishes the boundaries that future filesystem and device work can build on safely.

## Phase 1 - VFS Boundary Cleanup
 - [ ] **Clarify generic filesystem responsibilities**: Keep path traversal, file descriptor dispatch, common inode/file abstractions, and mount-point handling in the VFS layer.
 - [ ] **Move backend-specific behavior behind filesystem operations**: Ensure Easy-FS, EXT4, and future filesystems provide their behavior through VFS-facing operation tables instead of leaking layout details into generic code.
 - [ ] **Remove contest-era shortcuts**: Replace temporary EXT4/Easy-FS branches in generic paths with normal backend dispatch.

## Phase 2 - Filesystem Backend Separation
 - [ ] **Make Easy-FS a self-contained backend**: Keep Easy-FS allocation, inode persistence, directory handling, and file data mapping inside the Easy-FS implementation.
 - [ ] **Make EXT4 a formal read-only backend**: Preserve the v0.6 contest reader while exposing it through the same VFS-facing model as other filesystems.
 - [ ] **Keep shared infrastructure generic**: Restrict common block and inode cache code to filesystem-independent caching, locking, and lifecycle responsibilities.

## Phase 3 - devtmpfs and Device Files
 - [ ] **Introduce devtmpfs**: Add an in-memory filesystem for kernel-created device nodes.
 - [ ] **Move `/dev/tty` out of the mock VFS tree**: Represent the console as a real device file reachable through normal pathname lookup.
 - [ ] **Unify device I/O with file I/O**: Route console read/write through the same file operation path used by regular files.

## Phase 4 - Mount and Boot Integration
 - [ ] **Separate root filesystem and device filesystem**: Allow the boot rootfs and `/dev` to come from different filesystem backends.
 - [ ] **Preserve existing boot paths**: Keep the Easy-FS fallback, OpenSBI boot path, and EXT4 contest runner working while the architecture is cleaned up.
 - [ ] **Prepare for future mount support**: Establish enough internal mount structure to support later user-visible mount and umount work.

## Phase 5 - Validation and Documentation
 - [ ] **Regression test core boot flows**: Verify local Easy-FS boot, OpenSBI boot, and EXT4 contest runner behavior after the split.
 - [ ] **Regression test device I/O**: Verify stdio and `/dev/tty` behavior through devtmpfs.
 - [ ] **Document the new boundaries**: Update roadmap and architecture notes so future filesystem work follows the new VFS/backend split.

---

# 🚀 Roadmap (v0.6 - Contest Bootstrapping Milestone)

v0.6 moves FrostVista from a self-contained Easy-FS demo environment toward the contest evaluator. The milestone is intentionally narrow: boot the submitted `kernel-rv` in the evaluator's RISC-V QEMU environment, read the provided EXT4 test disk, run tests in a controlled sequence, and shut down.

This milestone deliberately defers the full devtmpfs cleanup and broader architecture boundary work. The current mock `/dev/tty` path remains in place until the contest boot and runner path is working.

## Phase 1 - Contest Boot Path
 - [x] **OpenSBI entry**: Support `-bios default -kernel kernel-rv`, where the kernel starts from OpenSBI in S-mode rather than from the local M-mode `-bios none` path.
 - [x] **Local boot compatibility**: Keep `make qemu` usable for fast local runs while making the contest path the release target.
 - [x] **Shutdown validation**: Use SBI SRST under OpenSBI and keep the QEMU `sifive,test` fallback only for local `-bios none` development.
 - [x] **Build artifact contract**: Keep `make all` build-only and make sure it emits `kernel-rv` without launching QEMU.

## Phase 2 - Minimal Read-Only EXT4
 - [x] **EXT4 mount probe**: Detect the evaluator's EXT4 image on virtio disk `x0` without relying on a partition table.
 - [x] **Metadata reader**: Parse the superblock, block group descriptor, inode table location, and root inode.
 - [x] **Root directory scan**: Enumerate root directory entries to discover scripts and executable test files.
 - [x] **Extent-backed reads**: Read regular file contents through the common simple extent cases needed by contest binaries and scripts.

## Phase 3 - ELF Loading From Contest Disk
 - [x] **Reader-based ELF loader**: Split ELF loading from Easy-FS `readi()` so the loader can consume either Easy-FS or EXT4-backed files.
 - [x] **Single binary execution**: Execute one selected ELF directly from EXT4 before adding script discovery.
 - [x] **Preserve Easy-FS fallback**: Keep the current `/init` Easy-FS path for local regression testing while the contest path is developed.

## Phase 4 - Contest Runner
 - [x] **Marker output**: Print the current `basic-musl` group start/end markers exactly in the runner output.
 - [x] **Serial execution**: Run selected tests one at a time through the existing `fork`/`exec`/`wait` lifecycle.
 - [x] **Final shutdown**: Call the shutdown path after the selected runner list finishes.
 - [x] **Embedded init runner**: Generate `build/gen/kernel/init_code.h` from the selected user test and use it as the `/init` image when present.

## Phase 5 - Test-Driven Syscall Fill
 - [x] **Basic syscall expansion**: Add only the syscalls required by failing tests.
 - [x] **Low-risk basic batch**: Pass or exercise the current low-risk runner set: `brk`, `getpid`, `getppid`, `write`, `exit`, `fork`, `wait`, `uname`, `times`, `gettimeofday`, `yield`, and `sleep`.
 - [x] **ABI visibility**: Add Linux RISC-V numbers for the next file, directory, VM, and mount-related tests, with explicit `LOG_ERROR` stubs for calls that are not implemented yet.
 - [x] **File descriptor semantics**: Finish the `close`, `fstat`, `open/openat`, `read`, and `dup3`/`dup2` behavior exposed by the next runner batch.

## Other
 - [x] **Superblock and feature gate**: Read the EXT4 superblock at byte offset 1024, validate magic `0xEF53`, record core layout fields, and reject unsupported incompatible features.
 - [x] **Group descriptor and root inode**: Read group 0's inode table location, load inode #2, and verify that the root directory uses extent-backed storage.
 - [x] **Root directory enumeration**: Parse the root directory's depth-0 extent and print `ext4_dir_entry_2` records from the first directory data block.
 - [x] **Local image target**: Add `make run-sbi-ext4 EXT4_IMG=sdcard-rv.img` to boot with an official EXT4 image as virtio `x0`.
 - [x] **EXT4-backed exec path**: Wire EXT4 lookup/read support into the VFS and ELF loading path so `/init` can be resolved from the contest image instead of Easy-FS only.
 - [x] **BusyBox reaches syscall dispatch**: Launch the contest image BusyBox far enough to expose missing syscall coverage as the next blocker.
 - [x] **Restore kernel `tp` on user trap**: Fix the trap entry path where musl uses `tp` as user TLS, while the kernel expects `tp` to hold the hart id for `cpuid()`/`get_cpu()`. On the current single-hart target, `uservec` restores `tp = 0` before entering C trap handling.
 - [x] **Separate DRAM base from kernel load base**: Keep `PHYSTOP_LOW` anchored to QEMU virt RAM at `0x80000000 + 128 MiB`, while allowing the OpenSBI kernel entry/load base to be `0x80200000`.
 - [x] **Fix `kalloc_init` access fault**: Prevent `freerange()` from releasing pages up to `0x88200000`, which incorrectly treated the OpenSBI 2 MiB entry offset as additional RAM and caused `memset()` to store beyond physical memory at `0x88000000`.
 - [x] **Verified OpenSBI boot**: `make -B run-sbi LOG=TRACE` reaches `kalloc_init end`, runs the user `argc` test, and enters `sys_shutdown`.
 - [x] **Embedded basic runner smoke**: `test/test_runner.c` now launches selected `/musl/basic/*` binaries from the EXT4 image, prints `basic-musl` markers, and shuts down after the list completes.
 - [x] **Openat ABI correction**: Route syscall 56 through `openat(dirfd, path, flags, mode)` argument decoding instead of the old `open(path, flags)` layout, eliminating the high-address access pattern caused by treating `AT_FDCWD` as a path pointer.
 - [x] **Contest runner path**: Add the minimal boot, filesystem, runner, and shutdown flow needed to scan and execute contest tests.

---

# 🚀 Roadmap (v0.5 - The Cleanup & Consolidation Milestone)

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
 - [x] **Fix inode private_data lifecycle**: Move allocation to `get_inode` and deallocation to `put_inode`, remove from `ilock`/`iunlock`. Eliminate `kalloc` leaks in `exec.c` and `mount.c`.
 - [x] **Move string helpers into core**: Relocate common string/memory routines from `kernel/mm` to `kernel/core`, matching their cross-subsystem role.
 - [x] **Harden `exec()` cleanup**: Ensure `ilock()` error paths release the inode, check `loadseg()` failures, and free the old address space without reporting a successful exec as an error.
 - [x] **Fix Easy-FS inode cleanup hazards**: Correct `itrunc()` so it frees only allocated direct blocks, and avoid copying whole cached inode structures during pathname traversal.
 - [x] **Harden syscall and file I/O edges**: Release `ftable_lock` on `open()` allocation failure, guard syscall logging against invalid numbers, validate file ops before read/write, and normalize negative device I/O returns to `-1`.
 - [x] **Remove VFS declaration warning**: Move `struct vfs_dirent` before `struct vfs_file_ops` so the kernel build no longer emits the forward declaration warning.
 - [x] **Lock documentation**: Document inode reference ownership, inode sleeplock acquire/release contracts, and buffer cache lock ownership for the core Easy-FS and bcache helpers.

---


# 🚀 Roadmap (v0.4 - The File System & I/O Milestone)
With memory and process lifecycles firmly established in v0.3, the v0.4 release bridges the gap to persistent storage and unified Unix I/O. This milestone breaks FrostVistaOS out of the "memory island," introducing a Virtual File System (VFS), block device caching, and standard file descriptors, laying the necessary groundwork for complex applications and future file systems like ext2.

## Phase 1 - Virtual File System (VFS) Abstraction
 - [x] **VFS Interface**: Define generic `inode`, `file`, and `superblock` structures to decouple the core kernel logic from specific file system implementations.
 - [x] **File Descriptor Table**: Implement a per-process FD table within `struct Process` to unify standard I/O, files, and devices under a single integer abstraction.
 - [x] **Core I/O Syscalls**: Implement the foundational Unix I/O interface, including `sys_open`, `sys_read`, `sys_write`, `sys_close`, and `sys_dup`.

## Phase 2 - Block Device & Buffer Cache
 - [x] **VirtIO Disk Driver**: Implement a VirtIO-compliant block device driver for QEMU to handle asynchronous disk read and write requests.
 - [x] **Buffer Cache (Block Cache)**: Develop an LRU-based memory cache for disk blocks to minimize slow I/O operations and manage concurrent block access using spinlocks/sleeplocks.
 - [x] **Interrupt-Driven I/O**: Utilize external interrupts to handle disk responses asynchronously, allowing the CPU to execute other processes instead of spinning.

## Phase 3 - Simple File System (Easy-FS)
 - [x] **On-Disk Layout**: Design a minimal file system backend for the VFS, featuring a superblock, block bitmap, inode array, and data blocks to validate the I/O pipeline.
 - [x] **Directory Operations**: Implement pathname translation, hierarchical directory entry management, and basic file creation/deletion logic.
 - [x] **File Data Management**: Map logical file offsets to physical disk blocks, ensuring safe appending and reading of file content.

## Phase 4 - Inter-Process Communication (IPC)
 - [x] **Standard I/O Redirection**: Link standard input, output, and error (FDs 0, 1, and 2) directly to the UART console driver for interactive user sessions.
 - [ ] **Anonymous Pipes**: (Moved to v0.6) Create a bounded ring-buffer mechanism in memory to allow byte-stream communication between processes.

---


# 🚀 Roadmap (v0.3 - The Userland & Lifecycle Milestone)
With the preemptive scheduler and context isolation achieved in v0.2, the v0.3 release shifts focus to true Unix process semantics, executable loading, and kernel concurrency protection. This transforms FrostVista from a task switcher into a full-fledged application host.

## Phase 1 - Unix Process Lifecycle
 - [x] Process Duplication (sys_fork): Implement the complex logic to deep-copy a parent process's page table, memory layout, and Trapframe into a new child process.
 - [x] Process Termination (sys_exit): Safely tear down a process, free its physical pages, close its resources, and transition it to a ZOMBIE state.
 - [x] Zombie Reaping (sys_wait): Allow parent processes to wait for child termination, fetch exit status, and cleanly scrub the PCB from the process table.
 - [x] Orphan Management: Implement logic to reparent orphaned child processes to the init process when their original parent dies first.
## Phase 2 - Executable Loading (ELF)
 - [x] ELF Format Parser: Write a lightweight parser to validate ELF magic numbers and read Program Headers.
 - [x] The Loader (sys_execve): Destroy the current process's memory space, allocate new pages, and map the executable's .text, .data, and .bss segments into U-mode memory.
 - [x] User Stack Initialization: Dynamically allocate a clean user stack and safely push argc, argv, and the initial stack frame before returning to U-mode.
 - [x] The init Process: Replace the hardcoded test payload with a compiled, standalone initcode loaded directly from memory or a basic RAM disk.
## Phase 3 - Dynamic User Memory
 - [x] Heap Expansion (sys_sbrk): Enable user programs to dynamically request more memory pages at runtime.
 - [x] Memory Accounting: Track the sz (size) of each process to prevent user-space from corrupting memory or growing into kernel space.
 - [x] Lazy Allocation: Modify the page fault trap handler to allocate physical memory only when the user program actually touches the heap, avoiding immediate kalloc overhead.
## Phase 4 - Concurrency & Synchronization
 - [x] Spinlocks (struct spinlock): Implement atomic amoswap-based locks to protect shared kernel data structures (like the memory pool and process array).
 - [x] Interrupt Control: Create reliable push_off() and pop_off() functions to safely disable hardware interrupts when entering critical sections, preventing deadlocks.
 - [x] Sleep & Wakeup Primitives: Implement condition variables to allow processes to sleep while waiting for I/O or child processes, without burning CPU cycles in a spin loop.
---

# 🚀 Roadmap (v0.2 - The Architecture & Process Milestone)

Building upon the solid Self-Hosting Memory Management achieved in v0.1, the v0.2 release will focus on architectural decoupling, introducing multitasking, and bridging the gap between User and Supervisor modes.

## **TODO: Phase 1 - Multi-Arch Refactoring**
- [x] **Hardware Abstraction Layer (HAL)**: Decouple generic kernel logic from hardware-specific instructions.
- [x] **Directory Restructuring**: Split the codebase into generic (`kernel/`, `include/`) and architecture-specific (`arch/riscv/`) directories.
- [x] **Smart Build System**: Upgrade the `Makefile` to dynamically compile sources based on the target architecture (e.g., `make ARCH=riscv`).

## **TODO: Phase 2 - System Call Infrastructure**
- [x] **Syscall Dispatcher**: Implement a robust delegation mechanism to handle `ecall` from U-mode, routing based on the `a7` register.
- [x] **Parameter Passing**: Safely extract arguments passed via user registers (`a0-a5`) into the kernel.
- [x] **First True Syscall (`sys_write`)**: Empower user programs to print messages to the UART terminal through the kernel.

## **TODO: Phase 3 - Process Management (PCB)**
- [x] **Process Control Block (`struct Process`)**: Define the core data structure to manage process state, PID, page tables, and dedicated kernel stacks.
- [x] **Process Allocator (`alloc_process`)**: Automate the creation pipeline (allocating memory, initializing the Trapframe, and setting up the process page table).
- [x] **Context Isolation**: Transition from a hardcoded "Mini User Mode" to dynamically allocated Trapframes for reliable register save/restore operations.

## **TODO: Phase 4 - Preemptive Scheduling**
- [x] **Timer Interrupt Finalization**: Complete the WIP timer interrupt handling to ensure a stable tick rate.
- [x] **Context Switcher (`swtch.S`)**: Write the critical assembly routine to swap CPU callee-saved registers between kernel threads/processes.
- [x] **Round-Robin Scheduler**: Implement the first CPU scheduler to multiplex execution time between multiple concurrent user processes.

## TODO
- [x] Clean up magic, dead code, spelling errors, and notebook-style comments

---

# 🚀 Release: (v0.1 - The Memory Milestone)

The FrostVistaOS kernel has successfully achieved **Self-Hosting Memory Management**, laying down a robust, modern, and secure foundation for future architectural expansion.

## **Completed: Phase 1 - Boot & Hardware Foundations**
- [x] **UART Driver**: Implemented MMIO-based serial output for reliable kernel logging and debugging.
- [x] **Boot Memory Allocator**: Engineered a bump-pointer allocator (`ekalloc`) dedicated to early-stage page table creation and initialization.

## **Completed: Phase 2 - Virtual Memory Architecture**
- [x] **Sv39 Paging**: Fully implemented 3-level page tables strictly compliant with the RISC-V Sv39 hardware standard.
- [x] **Higher-Half Mapping**: Successfully relocated and mapped the entire kernel virtual space to `0xFFFFFFC080000000`.
- [x] **The "Leap of Faith"**: Executed a deterministic and safe execution transition from the physical PC to the high-virtual PC.
- [x] **Mapping Cleanup**: Enforced strict memory isolation by dynamically destroying the early identity mapping scaffolding after boot (`kernel_table[0] = 0;`).
- [x] **Memory Semantics Annotation**: Annotated all core memory functions with explicit address requirements (High/Low VA or PA) to prevent pointer corruption.

## **Completed: Phase 3 - Privilege & Interrupts (The Gateway)**
- [x] **Mini User Mode**: Successfully executed a minimal context switch, dropping privileges from Supervisor (S) mode down to User (U) mode.
- [x] **UART Interrupts Handling**: Enabled and processed asynchronous UART interrupts, paving the way for interactive I/O.
- [x] **Trap & Interrupts Foundation**: Established the architectural baseline for timer and external hardware interrupts handling (WIP towards v0.2).

## TODO

- [x] **Memory Semantics Annotation**: Annotated all core memory functions with strict address requirements (High/Low VA or Physical Address) to prevent pointer corruption.
