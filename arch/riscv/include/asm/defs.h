#ifndef __ASM_DEFS_H__
#define __ASM_DEFS_H__

#include "kernel/types.h"

// syscall.c
void syscall();

// trap.c
void trapinit();
void usertrap();

// vm.c
void clear_low_memory_mappings();
void kvminithart();
void kvminit();
int mappages(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm);
int kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm);
uint64 walk_addr(pagetable_t pagetable, uint64 va);
void kvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free);
int uvmunmap(pagetable_t pagetable, uint64 va, int npage, int do_free);
int uvmcopy(pagetable_t old, pagetable_t new);

#endif // !__ASM_DEFS_H__
