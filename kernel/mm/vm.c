#include "driver/uart.h"
#include "kernel/kalloc.h"
#include "kernel/machine.h"
#include "kernel/mm.h"
#include "kernel/printf.h"
#include "kernel/riscv.h"
#include "kernel/string.h"
#include "kernel/types.h"

extern char _divide[];

pagetable_t mapping() {
  pagetable_t kernel_table;
  kernel_table = (uint64 *)kalloc();
  memset(kernel_table, 0, PGSIZE);

  mapp(kernel_table, UART0_BASE, UART0_BASE, PGSIZE, PTE_R | PTE_W);
  mapp(kernel_table, KERNEL_BASE, KERNEL_BASE, (uint64)_divide - KERNEL_BASE,
       PTE_R | PTE_X);
  mapp(kernel_table, (uint64)_divide, (uint64)_divide,
       PHYSTOP - (uint64)_divide, PTE_R | PTE_W);

  return kernel_table;
}

pte_t *walk(pagetable_t pagetable, uint64 va, int alloc) {

  for (int i = 3 - 1; i > 0; i--) {
    pte_t *pte = &pagetable[(va >> (12 + (9 * i))) & 0x1ff];
    if (*pte & PTE_V) {
      pagetable = (pagetable_t)(((*pte) >> 10) << 12);
    } else {
      if (!alloc || (pagetable = (pte_t *)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = ((((uint64)pagetable) >> 12) << 10) | PTE_V;
    }
  }
  return &pagetable[(va >> (12 + (9 * 0))) & 0x1ff];
}

int mapp(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm) {

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

void map_start() {
  sfence_vma();
  pagetable_t table = mapping();
  w_satp((8L << 60) | ((uint64)table) >> 12);
  sfence_vma();

  kprintf("Paging enable successfully\n");
}
