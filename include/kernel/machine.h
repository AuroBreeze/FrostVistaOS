#ifndef MACHINE_H
#define MACHINE_H

#define KERNEL_BASE 0x80000000UL
#define PHYSTOP (KERNEL_BASE + 128 * 1024 * 1024)

#define KERNEL_VIRT_OFFSET 0xFFFFFFC000000000UL // Hight Address Offset
#define VA_HIGHT(va) (va + KERNEL_VIRT_OFFSET) // Lower Address to Hight Address

#define PGSIZE 4096

#endif
