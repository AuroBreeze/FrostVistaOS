# FrostVista OS / 霜见内核 ❄️

```text
    ______                __ _     ___      __        
   / ____/________  _____/ /| |   / (_)____/ /_____ _
  / /_  / ___/ __ \/ ___/ __/ |  / / / ___/ __/ __ `/
 / __/ / /  / /_/ (__  ) /_ | | / / (__  ) /_/ /_/ / 
/_/   /_/   \____/____/\__/ |___/_/____/\__/\__,_/   
                                                     
FrostVistaOS booting...
Hello FrostVista OS!
Paging enable successfully
Successfully jumped to high address!
Current SP: 0xffffffc080006020
```

FrostVista is a lightweight, educational operating system kernel targeting **RISC-V 64 (Sv39)**.

Unlike typical hobby kernels that stay in physical memory, FrostVista implements a **Higher Half Kernel** architecture. It boots in M-mode, enables paging, cleans up identity mappings, and executes strictly in the upper virtual address space (`0xFFFFFFC080000000`).

# 🚀 Roadmap (v0.2 - The Architecture & Process Milestone)

Building upon the solid Self-Hosting Memory Management achieved in v0.1, the v0.2 release will focus on architectural decoupling, introducing multitasking, and bridging the gap between User and Supervisor modes.

Please refer to it for specific features: releases.md

## **TODO: Phase 1 - Multi-Arch Refactoring**
- [x] **Hardware Abstraction Layer (HAL)**: Decouple generic kernel logic from hardware-specific instructions.
- [x] **Directory Restructuring**: Split the codebase into generic (`kernel/`, `include/`) and architecture-specific (`arch/riscv/`) directories.
- [x] **Smart Build System**: Upgrade the `Makefile` to dynamically compile sources based on the target architecture (e.g., `make ARCH=riscv`).

## **TODO: Phase 2 - System Call Infrastructure**
- [x] **Syscall Dispatcher**: Implement a robust delegation mechanism to handle `ecall` from U-mode, routing based on the `a7` register.
- [x] **Parameter Passing**: Safely extract arguments passed via user registers (`a0-a5`) into the kernel.
- [x] **First True Syscall (`sys_write`)**: Empower user programs to print messages to the UART terminal through the kernel.

## **TODO: Phase 3 - Process Management (PCB)**
- [ ] **Process Control Block (`struct Process`)**: Define the core data structure to manage process state, PID, page tables, and dedicated kernel stacks.
- [ ] **Process Allocator (`alloc_process`)**: Automate the creation pipeline (allocating memory, initializing the Trapframe, and setting up the process page table).
- [ ] **Context Isolation**: Transition from a hardcoded "Mini User Mode" to dynamically allocated Trapframes for reliable register save/restore operations.

## **TODO: Phase 4 - Preemptive Scheduling**
- [ ] **Timer Interrupt Finalization**: Complete the WIP timer interrupt handling to ensure a stable tick rate.
- [ ] **Context Switcher (`swtch.S`)**: Write the critical assembly routine to swap CPU callee-saved registers between kernel threads/processes.
- [ ] **Round-Robin Scheduler**: Implement the first CPU scheduler to multiplex execution time between multiple concurrent user processes.

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
