Section:
    3.1.15 : exception/interrupt cause
    3.1.6 : mstatus.MIE
    3.1.9 : mip.MTIP / mip.MTIE
    3.2.1 : mtime / mtimecmp / mie.MTIE enable
    12.3 : Sv32/satp.Mode/pte detail/va->pa/
    12.1 : satp detail/satp.ppn/SPIE

```
Low Address (e.g., 0x00000)
    +-----------------------+ 
    |   (Kernel Reserved)   |
    +-----------------------+ <---- 0x10000 (Entry Point)
    |      Text (Code)      |
    +-----------------------+
    |      Data / BSS       |       (Global variables, etc.)
    +-----------------------+ 
    | Guard Page (Unmapped) |       (Prevents stack overflow)
    +-----------------------+
    |      User Stack       |       (Fixed size during exec)
    +-----------------------+ <---- current_proc->heap_bottom (Initial size)
    |                       |
    | Heap (Dynamic Memory) |       (Managed by sbrk)
    |   (Lazy Allocation)   |
    |       | | | | |       |
    |       v v v v v       |       (Grows towards higher addresses)
    +-----------------------+ <---- current_proc->size (Current Heap Top)
    |                       |
    |  (Unallocated Space)  |
    |                       |
    +-----------------------+ 
    High Address (e.g., MAXVA)
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
