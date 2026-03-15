#ifndef _ASM_VM_H
#define _ASM_VM_H

#include "kernel/types.h"
// vm.c
void clear_low_memory_mappings();
void kvminithart();
void kvminit();
int mappages(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm);
int kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm);
uint64 walk_addr(pagetable_t pagetable, uint64 va);
void kvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free);

#endif
