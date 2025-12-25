# 🚀 Current Status (v0.1 - Memory Milestone)

The kernel has successfully achieved **Self-Hosting Memory Management**:

- [x] **UART Driver**: MMIO based serial output.
- [x] **Boot Memory Allocator**: Simple bump-pointer allocator (`ekalloc`) for early page table creation.
- [x] **Sv39 Paging**: 3-level page tables with Sv39 standard.
- [x] **Higher Half Mapping**: Kernel mapped to `0xFFFFFFC080000000`.
- [x] **The "Leap of Faith"**: Safe transition from physical PC to high-virtual PC.
- [x] **Cleanup**: Identity mappings are removed after boot for a clean virtual space.
- [x] **Trap & Interrupts**: (Work In Progress) Timer and external interrupts.
- [ ] **UART Interrupts Handling**
- [ ] **Mini User Mode**: Minimal implementration from S mode to U mode.
- [ ] **Process Management and Scheduling**

## **TODO**
- [ ] Implement a simple logging system to record instances where "return 0" occurs, facilitating error analysis.
- [ ] To achieve a more robust trap function or a more user-friendly logging system, it is necessary to do document the system's pagination, mapping, permissions, and other configurations. This facilitates subsequent issue tracking and analysis.

