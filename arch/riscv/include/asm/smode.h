#ifndef FV_RISCV_INCLUDE_ASM_SMODE_H
#define FV_RISCV_INCLUDE_ASM_SMODE_H

#include "kernel/types.h"

extern pagetable_t kernel_table; // kernel page table
extern char _kernel_end[];	 // End address of the kernel
extern char
    *ekalloc_ptr; // Memory addresses used in the initial memory allocation

extern int early_mode;

#endif
