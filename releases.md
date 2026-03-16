## 🎯 TODO
- [x] Clean up magic, dead code, spelling errors, and notebook-style comments
- [ ] **Kernel Logging Subsystem**: Implement a robust logging system to capture warning/error states (e.g., unexpected `return 0` instances) to facilitate deep error analysis.
- [ ] **Architecture Documentation**: Comprehensively document the system's paging mechanism, high-half mapping layout, and privilege configurations. This is critical for building a robust trap handler and facilitating future issue tracking.


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
- [ ] **Process Control Block (`struct Process`)**: Define the core data structure to manage process state, PID, page tables, and dedicated kernel stacks.
- [ ] **Process Allocator (`alloc_process`)**: Automate the creation pipeline (allocating memory, initializing the Trapframe, and setting up the process page table).
- [ ] **Context Isolation**: Transition from a hardcoded "Mini User Mode" to dynamically allocated Trapframes for reliable register save/restore operations.

## **TODO: Phase 4 - Preemptive Scheduling**
- [ ] **Timer Interrupt Finalization**: Complete the WIP timer interrupt handling to ensure a stable tick rate.
- [ ] **Context Switcher (`swtch.S`)**: Write the critical assembly routine to swap CPU callee-saved registers between kernel threads/processes.
- [ ] **Round-Robin Scheduler**: Implement the first CPU scheduler to multiplex execution time between multiple concurrent user processes.

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

