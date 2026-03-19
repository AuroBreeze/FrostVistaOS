# FrostVista OS / 霜见内核 ❄️

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

# 🚀 Roadmap (v0.3 - The Userland & Lifecycle Milestone)

With the preemptive scheduler and context isolation achieved in v0.2, the v0.3 release shifts focus to true Unix process semantics, executable loading, and kernel concurrency protection. This transforms FrostVista from a task switcher into a full-fledged application host.

## Phase 1 - Unix Process Lifecycle
 - [x] Process Duplication (sys_fork): Implement the complex logic to deep-copy a parent process's page table, memory layout, and Trapframe into a new child process.
 - [x] Process Termination (sys_exit): Safely tear down a process, free its physical pages, close its resources, and transition it to a ZOMBIE state.
 - [x] Zombie Reaping (sys_wait): Allow parent processes to wait for child termination, fetch exit status, and cleanly scrub the PCB from the process table.
 - [ ] Orphan Management: Implement logic to reparent orphaned child processes to the init process when their original parent dies first.
 ## Phase 2 - Executable Loading (ELF)
 - [ ] ELF Format Parser: Write a lightweight parser to validate ELF magic numbers and read Program Headers.
 - [ ] The Loader (sys_execve): Destroy the current process's memory space, allocate new pages, and map the executable's .text, .data, and .bss segments into U-mode memory.
 - [ ] User Stack Initialization: Dynamically allocate a clean user stack and safely push argc, argv, and the initial stack frame before returning to U-mode.
 - [ ] The init Process: Replace the hardcoded test payload with a compiled, standalone initcode loaded directly from memory or a basic RAM disk.
 ## Phase 3 - Dynamic User Memory
 - [ ] Heap Expansion (sys_sbrk): Enable user programs to dynamically request more memory pages at runtime.
 * [ ] Memory Accounting: Track the sz (size) of each process to prevent user-space from corrupting memory or growing into kernel space.
 - [ ] Lazy Allocation: Modify the page fault trap handler to allocate physical memory only when the user program actually touches the heap, avoiding immediate kalloc overhead.
## Phase 4 - Concurrency & Synchronization
 - [ ] Spinlocks (struct spinlock): Implement atomic amoswap-based locks to protect shared kernel data structures (like the memory pool and process array).
 - [ ] Interrupt Control: Create reliable push_off() and pop_off() functions to safely disable hardware interrupts when entering critical sections, preventing deadlocks.
 - [ ] Sleep & Wakeup Primitives: Implement condition variables to allow processes to sleep while waiting for I/O or child processes, without burning CPU cycles in a spin loop.

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
