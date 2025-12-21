#include "driver/uart.h"
#include "kernel/defs.h"
#include "kernel/machine.h"
#include "kernel/mm.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

extern char _divide[];
extern char _kernel_end[];
pagetable_t kernel_table;

// #define DEBUG

pagetable_t kvmmake() {
  pagetable_t pagetable;
  pagetable = (pagetable_t)ekalloc();
#ifdef DEBUG
  kprintf("pagetable_t: %p\n", pagetable);
#endif
  memset(pagetable, 0, PGSIZE);

  kvmmap(pagetable, UART0_BASE, UART0_BASE, PGSIZE, PTE_R | PTE_W);
  kvmmap(pagetable, KERNEL_BASE_LOW, KERNEL_BASE_LOW,
         (uint64)_divide - KERNEL_BASE_LOW, PTE_R | PTE_X);
  kvmmap(pagetable, (uint64)_divide, (uint64)_divide,
         PHYSTOP_LOW - (uint64)_divide, PTE_R | PTE_W);

  // Hight Address mapping
  kvmmap(pagetable, KERNEL_BASE_HIGH, KERNEL_BASE_LOW,
         // NOTE: The address _divide here is not a high address,
         // because at this point it is merely mapped and has not
         // yet been jumped to via SP to load _divide
         ADR2HIGHT((uint64)_divide) - KERNEL_BASE_HIGH, PTE_R | PTE_X);

  kvmmap(pagetable, ADR2HIGHT((uint64)_divide), (uint64)_divide,
         PHYSTOP_HIGH - ADR2HIGHT((uint64)_divide), PTE_R | PTE_W);
#ifdef DEBUG
  kprintf("\nmapping high addresses:\nhigh va: %p to pa: %p\nsize: %x\n",
          (void *)KERNEL_BASE_HIGH, (void *)KERNEL_BASE_LOW,
          ADR2HIGHT((uint64)_divide) - KERNEL_BASE_HIGH);

  kprintf("\nmapping high addresses:\nhigh va: %p to pa: %p\nsize: %x\n",
          (void *)ADR2HIGHT((uint64)_divide), (void *)_divide,
          PHYSTOP_HIGH - ADR2HIGHT((uint64)_divide));
#endif

  return pagetable;
}

void kvminit() { kernel_table = kvmmake(); }

// Refresh the TLB and enable paging
void kvminithart() {
  sfence_vma();
  w_satp((8L << 60) | (uint64)(kernel_table) >> 12);
  sfence_vma();

  kprintf("Paging enable successfully\n");
}

// NOTE: Requires virtual high addresses
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc) {
  // WARNING: Pay attention to the range of VA adresses
  for (int i = 3 - 1; i > 0; i--) {
    pte_t *pte = &pagetable[VPN_GET(va, i)];
    if (*pte & PTE_V) {
      pagetable = (pagetable_t)(PTE2PA(*pte));
    } else {
      if (!alloc || (pagetable = (pte_t *)pt_alloc_page_pa()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[VPN_GET(va, 0)];
}

// Look up the physical address mapped to the va in the  pagetable
// NOTE: Requires virtual high addresses
uint64 walk_addr(pagetable_t pagetable, uint64 va) {
  // WARNING: Pay attention to the range of VA adresses
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0)
    return 0;
  if ((*pte & PTE_V) == 0) {
    return 0;
  }
  // TODO: User-mode verification may be required

  uint64 pa;
  pa = PTE2PA(*pte);
  return pa;
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
    if ((pte = walk(pagetable, a, 1)) == 0) {
#ifdef DEBUG
      kprintf("WARNING: no memory\n");
#endif
      return 0;
    }

    if (*pte & PTE_V)
      panic("mappages: remap");

    *pte = ((((uint64)pa) >> 12) << 10) | perm | PTE_V;
    if (a == last) {
#ifdef DEBUG
      kprintf("va: %p pa: %p, perm: w: %d, r: %d, v:%d\n", (void *)a,
              (void *)pa, PTE_W & *pte, PTE_R & *pte, PTE_V & *pte);
#endif

      break;
    }

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

// Ensure that the size is aligned to PGSIZE
void kvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free) {
  if (va % PGSIZE != 0 || size % PGSIZE != 0) {
    panic("kvmunmap: va not aligned");
  }

  pte_t *pte;
  uint64 a = va;
  for (; va < a + size; va += PGSIZE) {
    if ((pte = walk(pagetable, va, 0)) == 0) {
      panic("kvmunmap: walk failed");
    }
    if ((*pte & PTE_V) == 0) {
      panic("kvmunmap: not mapped");
    }
    if (PTE_FLAGS(*pte) == PTE_V) {
      panic("kvmunmap: not a leaf");
    }

    if (do_free) {
      kfree((void *)PTE2PA(*pte));
    }
    *pte = 0;
  }
}
