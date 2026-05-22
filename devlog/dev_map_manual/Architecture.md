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


Block  0       1       2       3       4 ... 10     11      12 ... (11+N)  ... 9999
      +-------+-------+-------+-------+------------+-------+----------------+--------+
      | Boot  | Super | IBmap | DBmap |   Inode    | Root  |  Init binary   |  Free  |
      |(empty)| Block |       |       |   Area     | Dir   |  (N blocks)    |  Data  |
      +-------+-------+-------+-------+------------+-------+----------------+--------+
         1       1       1       1      7 blocks      1      N blocks      reset
                                        (448 inodes)       (N = init_blocks_needed, ≤10)
```

## EXT4 extent reader bring-up

Current scope: read-only probe for the contest EXT4 image. It supports the
simple `depth = 0` extent layout first, where the inode contains the extent
header and leaf extent entries directly inside `i_block[60]`.

```text
root inode (#2)
└── i_block[60]                         // inode 内部 60 字节，不是磁盘数据块
    ├── ext4_extent_header, 12 bytes
    │   ├── magic   = 0xf30a            // extent tree magic
    │   ├── entries = 1                 // 当前实际使用的 entry 数量
    │   ├── max     = 4                 // i_block[60] 中最多能容纳 4 个 entry
    │   └── depth   = 0                 // 叶子节点，entry 直接指向数据块
    │
    ├── ext4_extent[0], 12 bytes        // 有效，因为 entries = 1
    │   ├── ee_block = 0                // 文件内逻辑块号
    │   ├── ee_len   = 1                // 连续块数量
    │   └── ee_start = 8737             // 文件系统物理块号
    │
    ├── ext4_extent[1], 12 bytes        // 未使用
    ├── ext4_extent[2], 12 bytes        // 未使用
    └── ext4_extent[3], 12 bytes        // 未使用


physical block 8737
└── root directory data
    ├── ext4_dir_entry_2 "."          -> inode #2
    ├── ext4_dir_entry_2 ".."         -> inode #2
    ├── ext4_dir_entry_2 "lost+found" -> inode #11
    ├── ext4_dir_entry_2 "glibc"      -> inode #131073
    └── ext4_dir_entry_2 "musl"       -> inode #12
```

The next lookup step follows the directory entry inode number and repeats the
same inode -> extent -> data block process:

```text
root inode (#2)
└── i_block[60]
    └── ext4_extent[0]
        └── physical block 8737
            └── dirent "musl" -> inode #12


musl inode (#12)
└── i_block[60]
    ├── ext4_extent_header
    │   ├── entries = 1
    │   ├── max     = 4
    │   └── depth   = 0
    │
    └── ext4_extent[0]
        ├── ee_block = 0
        ├── ee_len   = 1
        └── ee_start = 8743


physical block 8743
└── /musl directory data
    ├── ext4_dir_entry_2 "."       -> inode #12
    ├── ext4_dir_entry_2 ".."      -> inode #2
    ├── ext4_dir_entry_2 "busybox" -> inode #55
    ├── ext4_dir_entry_2 "basic"   -> inode #16
    ├── ext4_dir_entry_2 "lib"     -> inode #76
    └── ...
```

Important naming detail: `struct ext4_inode_min::block` is a copy of EXT4
`i_block[60]`. It is not itself a physical block number. In extent mode
(`EXT4_EXTENTS_FL`), these 60 bytes start with `ext4_extent_header`; the
physical data block number is decoded later from `ext4_extent.ee_start`.
