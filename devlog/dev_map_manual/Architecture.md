```
Low Address (0x00000000)
    +-----------------------+ 
    |                       |
    |   (Unmapped Space)    |       (Catches NULL pointer dereferences)
    |                       |
    +-----------------------+ <---- 0x10000 (ELF Entry Point)
    |      Text (Code)      |
    +-----------------------+
    |      Data / BSS       |       (Global variables, etc.)
    +-----------------------+ <---- sz (End of ELF segments)
    | Guard Page (Unmapped) |       (Prevents heap from corrupting Data/BSS)
    +-----------------------+ <---- current_proc->heap_bottom (sz + PGSIZE)
    |                       |
    | Heap (Dynamic Memory) |       (Managed by sbrk, purely logical size)
    |   (Lazy Allocation)   |       (Physical pages allocated on Page Fault)
    |       | | | | |       |
    |       v v v v v       |       (Grows towards higher addresses)
    +-----------------------+ <---- current_proc->heap_top
    |                       |
    |  (Unallocated Space)  |       (Invalid access causes process to ZOMBIE)
    |                       |
    +-----------------------+ <---- current_proc->stack_bottom (PHYSTOP_LOW - PGSIZE)
    |      User Stack       |       (Fixed 1-Page size, logically grows downwards)
    +-----------------------+ <---- current_proc->stack_top (PHYSTOP_LOW)
    |                       |
    |  (Kernel Pagetable)   |       (Mapped in high memory, PDE 256-511)
    |                       |
    +-----------------------+ 
High Address (MAXVA)
```



```
Virtual Address (39-bit)
├── VPN[2] : 9 bits (Index for Level 2 Page Table)
├── VPN[1] : 9 bits (Index for Level 1 Page Table)
├── VPN[0] : 9 bits (Index for Level 0 Page Table)
└── Offset : 12 bits (Byte offset within 4KB Page)

satp (Supervisor Address Translation and Protection)
└── PPN : 44 bits (Physical Page Number of Root PT)
    │
    │ [Address = satp.PPN << 12]
    │
    └── PageTable (Level 2 - Root)
        ├── entries[0] ──> PTE
        ├── ...
        ├── entries[VPN2] ──> PTE (Pointer to Next Level)
        │                 ├── PPN : 44 bits ──> [Address = PTE.PPN << 12]
        │                 │                    │
        │                 │                    └── PageTable (Level 1)
        │                 │                        ├── entries[0] ──> ...
        │                 │                        ├── entries[VPN1] ──> PTE (Pointer to Next Level)
        │                 │                        │                 ├── PPN : 44 bits ──> [Address = PTE.PPN << 12]
        │                 │                        │                 │                    │
        │                 │                        │                 │                    └── PageTable (Level 0 - Leaf)
        │                 │                        │                 │                        ├── entries[0] ──> ...
        │                 │                        │                 │                        ├── entries[VPN0] ──> PTE (Leaf Node)
        │                 │                        │                 │                        │                 ├── PPN : 44 bits (Target Phys Page)
        │                 │                        │                 │                        │                 │                    │
        │                 │                        │                 │                        │                 │                    │ [Mapped To]
        │                 │                        │                 │                        │                 │                    v
        │                 │                        │                 │                        │                 │             Physical Memory
        │                 │                        │                 │                        │                 │             +---------------+
        │                 │                        │                 │                        │                 │             | Physical Page |
        │                 │                        │                 │                        │                 │             | (4KB)         |
        │                 │                        │                 │                        │                 │             +---------------+
        │                 │                        │                 │                        │                 │             | Final Address |
        │                 │                        │                 │                        │                 │             | (PPN << 12)   |
        │                 │                        │                 │                        │                 │             | + Offset      |
        │                 │                        │                 │                        │                 │             +---------------+
        │                 │                        │                 │                        │                 │
        │                 │                        │                 │                        │                 ├── Flags : 10 bits
        │                 │                        │                 │                        │                 │   ├── V (Valid)
        │                 │                        │                 │                        │                 │   ├── R/W/X (Permissions)
        │                 │                        │                 │                        │                 │   └── U/G/A/D (Attributes)
        │                 │                        │                 │                        │                 └── Reserved : 10 bits
        │                 │                        │                 │                        └── entries[511]
        │                 │                        │                 │
        │                 │                        │                 ├── Flags (V=1, R/W/X != 0)
        │                 │                        │                 └── ...
        │                 │                        └── entries[511]
        │                 │
        │                 ├── Flags (V=1, R/W/X == 0)
        │                 └── ...
        └── entries[511]
```
