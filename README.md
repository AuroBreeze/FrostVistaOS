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

Unlike typical hobby kernels that stay in physical memory, FrostVista implements a **Higher Half Kernel** architecture. It boots in M-mode, enables paging, cleans up identity mappings, and executes strictly in the upper virtual address space (`0xFFFFFFC080000000`).

---

## 🚀 Roadmap (v0.4 - The File System & I/O Milestone)
With memory and process lifecycles firmly established in v0.3, the v0.4 release bridges the gap to persistent storage and unified Unix I/O. This milestone breaks FrostVistaOS out of the "memory island," introducing a Virtual File System (VFS), block device caching, and standard file descriptors, laying the necessary groundwork for complex applications and future file systems like ext2.

### Phase 1 - Virtual File System (VFS) Abstraction
 - [ ] **VFS Interface**: Define generic `inode`, `file`, and `superblock` structures to decouple the core kernel logic from specific file system implementations.
 - [ ] **File Descriptor Table**: Implement a per-process FD table within `struct Process` to unify standard I/O, files, and devices under a single integer abstraction.
 - [ ] **Core I/O Syscalls**: Implement the foundational Unix I/O interface, including `sys_open`, `sys_read`, `sys_write`, `sys_close`, and `sys_dup`.

### Phase 2 - Block Device & Buffer Cache
 - [ ] **VirtIO Disk Driver**: Implement a VirtIO-compliant block device driver for QEMU to handle asynchronous disk read and write requests.
 - [ ] **Buffer Cache (Block Cache)**: Develop an LRU-based memory cache for disk blocks to minimize slow I/O operations and manage concurrent block access using spinlocks/sleeplocks.
 - [ ] **Interrupt-Driven I/O**: Utilize external interrupts to handle disk responses asynchronously, allowing the CPU to execute other processes instead of spinning.

### Phase 3 - Simple File System (Easy-FS)
 - [ ] **On-Disk Layout**: Design a minimal file system backend for the VFS, featuring a superblock, block bitmap, inode array, and data blocks to validate the I/O pipeline.
 - [ ] **Directory Operations**: Implement pathname translation, hierarchical directory entry management, and basic file creation/deletion logic.
 - [ ] **File Data Management**: Map logical file offsets to physical disk blocks, ensuring safe appending and reading of file content.

### Phase 4 - Inter-Process Communication (IPC)
 - [ ] **Anonymous Pipes**: Create a bounded ring-buffer mechanism in memory to allow byte-stream communication between processes, essential for shell pipelines.
 - [ ] **Standard I/O Redirection**: Link standard input, output, and error (FDs 0, 1, and 2) directly to the UART console driver for interactive user sessions.

---

## 🛠 Memory Layout

FrostVista utilizes the Sv39 virtual addressing scheme:

```text
0xFFFFFFC080000000  ->  Kernel Base (Virtual)
        |                   maps to
0x0000000080000000  ->  Physical RAM Start
```

## 🏗 Build & Run

**Requirements:**

* `riscv64-unknown-elf-gcc` (or similar cross-compiler)
* `qemu-system-riscv64`
* `make`

**To build and launch QEMU:**

```bash
make run
```

You should see the kernel enabling paging and jumping to the higher half address space in the serial console.

## 📜 Philosophy

* **Clarity over Cleverness**: Code is written to be understood.
* **Architecture First**: Implementing proper OS concepts (Virtual Memory, Traps) rather than hacking features.
* **From Scratch**: Minimizing external dependencies to understand the hardware.

---
