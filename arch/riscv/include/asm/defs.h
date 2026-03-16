#ifndef __ASM_DEFS_H__
#define __ASM_DEFS_H__

#include "kernel/types.h"

// syscall.c
struct trapframe;
void syscall(struct trapframe *frame);

// trap.c
void trapinit();
void user_mode_start();

struct trapframe;
void usertrap(struct trapframe *);

// vm.c
void clear_low_memory_mappings();
void kvminithart();
void kvminit();
int mappages(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm);
int kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm);
uint64 walk_addr(pagetable_t pagetable, uint64 va);
void kvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free);

#endif // !__ASM_DEFS_H__
