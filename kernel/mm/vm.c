#include "driver/uart.h"
#include "kernel/kalloc.h"
#include "kernel/machine.h"
#include "kernel/mm.h"
#include "kernel/printf.h"
#include "kernel/riscv.h"
#include "kernel/string.h"
#include "kernel/types.h"

extern char _divide[];

pagetable_t kernel_table;

pagetable_t kvmmake() {
  pagetable_t pagetable;
  pagetable = (pagetable_t)kalloc();
  memset(pagetable, 0, PGSIZE);

  kvmmap(pagetable, UART0_BASE, UART0_BASE, PGSIZE, PTE_R | PTE_W);
  kvmmap(pagetable, KERNEL_BASE, KERNEL_BASE, (uint64)_divide - KERNEL_BASE,
         PTE_R | PTE_X);
  kvmmap(pagetable, (uint64)_divide, (uint64)_divide, PHYSTOP - (uint64)_divide,
         PTE_R | PTE_W);

  return pagetable;
}

pte_t *walk(pagetable_t pagetable, uint64 va, int alloc) {

  for (int i = 3 - 1; i > 0; i--) {
    pte_t *pte = &pagetable[VPN_GET(va, i)];
    if (*pte & PTE_V) {
      pagetable = (pagetable_t)(PTE2PA(*pte));
    } else {
      if (!alloc || (pagetable = (pte_t *)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[VPN_GET(va, 0)];
}

int mappages(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm) {

  if ((va % PGSIZE) != 0)
    panic("mappages: va not aligned");

  if ((size % PGSIZE) != 0)
    panic("mappages: size not aligned");

  if (size == 0)
    panic("mappages: size");

  uint64 a, last;
  pte_t *pte;

  a = va;
  last = va + size - PGSIZE;
  for (;;) {
    if ((pte = walk(pagetable, a, 1)) == 0)
      return -1;

    if (*pte & PTE_V)
      panic("mappages: remap");

    *pte = ((((uint64)pa) >> 12) << 10) | perm | PTE_V;
    if (a == last)
      break;

    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

int kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm) {
  if (mappages(pagetable, va, pa, size, perm)) {
    return 1;
  }
  return 0;
}

void kvminit() { kernel_table = kvmmake(); }

void kvminithart() {
  sfence_vma();
  w_satp((8L << 60) | ((uint64)kernel_table) >> 12);
  sfence_vma();

  kprintf("Paging enable successfully\n");
}
