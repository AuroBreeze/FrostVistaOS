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

## 🚀 Current Status (v0.1 - Memory Milestone)

The kernel has successfully achieved **Self-Hosting Memory Management**:

* [x] **UART Driver**: MMIO based serial output.
* [x] **Boot Memory Allocator**: Simple bump-pointer allocator (`ekalloc`) for early page table creation.
* [x] **Sv39 Paging**: 3-level page tables with Sv39 standard.
* [x] **Higher Half Mapping**: Kernel mapped to `0xFFFFFFC080000000`.
* [x] **The "Leap of Faith"**: Safe transition from physical PC to high-virtual PC.
* [x] **Cleanup**: Identity mappings are removed after boot for a clean virtual space.
* [x] **Trap & Interrupts**: (Work In Progress) Timer and external interrupts.
* [x] **UART Interrupts Handling**
* [ ] **Mini User Mode**: Minimal implementration from S mode to U mode.
* [ ] **Process Management and Scheduling**

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
