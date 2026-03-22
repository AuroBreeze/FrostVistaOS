#ifndef __ASM_DEFS_H__
#define __ASM_DEFS_H__

#include "kernel/types.h"
struct Process;

// trap.c
void trapinit();
void usertrap();

// vm.c
void clear_low_memory_mappings();
void kvminithart();
void kvminit();
int mappages(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm);
int kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm);
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
uint64 walk_addr(pagetable_t pagetable, uint64 va);
void kvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free);
void uvmunmap(pagetable_t pagetable, uint64 va, int npage, int do_free);
int uvmcopy(pagetable_t old, pagetable_t new);
void uvmunmap(pagetable_t pagetable, uint64 va, int npage, int do_free);
void freewalk(pagetable_t pagetable);
void uvmfree(pagetable_t pagetable, struct Process *p);
uint64 uvmalloc(pagetable_t pagetable, uint64 va, uint64 size, int perm);
uint64 uvmdealloc(pagetable_t pagetable, uint64 va, uint64 size);
pagetable_t uvmcreate();
int copyin(pagetable_t pagetabel, char *dst, uint64 src, int len);
int copyout(pagetable_t pagetable, char *dst, uint64 src, int len);
int is_cow_fault(pagetable_t pagetable, uint64 va);
int handle_cow_fault(pagetable_t pagetable, uint64 va);
int handle_page_fault(pagetable_t pagetable, uint64 va);

#endif // !__ASM_DEFS_H__
