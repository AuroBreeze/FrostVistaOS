## 🎯 TODO
- [ ] **Kernel Logging Subsystem**: Implement a robust logging system to capture warning/error states (e.g., unexpected `return 0` instances) to facilitate deep error analysis.
- [ ] **Set** “normal exit” to 0 and “error exit” to -1
- [ ] **Architecture Documentation**: Comprehensively document the system's paging mechanism, high-half mapping layout, and privilege configurations. This is critical for building a robust trap handler and facilitating future issue tracking.
---

# 🚀 Roadmap (v0.5 - The Cleanup & Consolidation Milestone)

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

