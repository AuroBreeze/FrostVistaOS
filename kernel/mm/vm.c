// FIXME: Handle the transition from early boot (low identity mapping) to
// high-half kernel mapping. VM functions must correctly return high virtual
// addresses (VA) instead of physical addresses (PA).

// TODO: Annotate all functions with their specific address requirements
// (High/Low VA or PA)

#include "driver/PLIC.h"
#include "driver/uart.h"
#include "kernel/defs.h"
#include "kernel/kalloc.h"
#include "kernel/log.h"
#include "kernel/machine.h"
#include "kernel/mm.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

extern char _divide[];
extern char _kernel_end[];
extern int early_mode;
pagetable_t kernel_table;

/**
 * clear_low_memory_mappings - CLear the identity mapping of the kernel area
 *
 * Context: After the kernel migrates to high-half area, clear the identity
 * mapping, but retain the mapping of drivers such as UART at low addresses.
 *
 * Return: void
 */
void clear_low_memory_mappings() {
  uint64 low_kernel_end = (uint64)_kernel_end;
  if (IS_ADR_HIGHT(_kernel_end)) {
    low_kernel_end = ADR2LOW(_kernel_end);
  }
  kvmunmap(kernel_table, KERNEL_BASE_LOW, (low_kernel_end - KERNEL_BASE_LOW),
           0);
  kvmunmap(kernel_table, (uint64)ekalloc_ptr,
           (PHYSTOP_LOW - (uint64)ekalloc_ptr), 0);
  sfence_vma();
}

/**
 * kvmmake - Create a kernel page table and map the kernel
 *
 * Context: When the kernel has just started and paging is not enabled, use a
 * simple allocator to create a kernel page table and map the necessary drivers
 *
 * Return: A pointer to the kernel page table
 */
pagetable_t kvmmake() {
  pagetable_t pagetable;
  pagetable = (pagetable_t)ekalloc();

  LOG_TRACE("pagetable_t: %p", pagetable);

  memset(pagetable, 0, PGSIZE);

  kvmmap(pagetable, UART0_BASE, UART0_BASE, PGSIZE, PTE_R | PTE_W);
  kvmmap(pagetable, PLIC_BASE, PLIC_BASE, PLIC_MM_SIZE, PTE_R | PTE_W | PTE_X);

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

  LOG_TRACE("\nmapping high addresses:\nhigh va: %p to pa: %p\nsize: %x",
            (void *)KERNEL_BASE_HIGH, (void *)KERNEL_BASE_LOW,
            ADR2HIGHT((uint64)_divide) - KERNEL_BASE_HIGH);

  LOG_TRACE("\nmapping high addresses:\nhigh va: %p to pa: %p\nsize: %x",
            (void *)ADR2HIGHT((uint64)_divide), (void *)_divide,
            PHYSTOP_HIGH - ADR2HIGHT((uint64)_divide));

  return pagetable;
}

/**
 * kvminit - Initialize the kernel page table
 */
void kvminit() { kernel_table = kvmmake(); }

/**
 * kvminithart - Enable paging
 *
 * Context: Once the kernel page table initialization is complete, the kernel
 * is no longer empty, and satp can written to enable paging
 *
 * Return: void
 */
void kvminithart() {
  sfence_vma();
  if (kernel_table == 0) {
    panic("kernel_table is null");
  }
  w_satp((8L << 60) | (uint64)(kernel_table) >> 12);
  sfence_vma();

  LOG_INFO("Paging enable successfully");
}

/**
 * walk - Look up the physical address mapped to the va in the pagetable
 * @pagetable : Base address of the target pagetable
 * @va : The VA that needs to be found
 * @alloc : Whether to allocate a new page
 *
 * Context: Obtain a pointer to the page table entry (PTE) corresponding to the
 * virtual address, and modify the corresponding location. If alloc is true,
 * allocate a new page if the PTE is not found
 *
 * Return: A pointer to the PTE
 */
// NOTE: Note that if OS is running in early mode, the address is still a low
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc) {
  for (int i = 3 - 1; i > 0; i--) {
    pte_t *pte = &pagetable[VPN_GET(va, i)];
    if (*pte & PTE_V) {
      uint64 pa = (PTE2PA(*pte));
      // NOTE: After the OS has finished booting, that is, after mapping and
      // paging are completed, although the identity mapping is not cleared
      // immediately, it should no longer be used to access physical addresses;
      // instead, VA, which is the high address mapped address, should be used.
      //
      // NOTE: But sometimes the brain just can't wrap around it; what you need
      // to remember is that after enabling paging, PA can no longer be used. To
      // use VA, VA has already been mapped earlier. The correspondence between
      // VA and PA is VA = ADR2HIGH(PA).
      // Moreover, the PPN must be a real physical address, so this is the root
      // cause of the page table translation.
      //
      // NOTE: In the high-half mapping
      // VA = ADR2HIGH(PA)
      // VPN = VA >> 12
      // PA = ADR2LOW(PA)
      // PPN = PA >> 12
      // PTE = (PPN << 10) + other
      pagetable = early_mode ? (pagetable_t)pa : (pagetable_t)ADR2HIGHT(pa);
    } else {
      if (!alloc || (pagetable = (pte_t *)pt_alloc_page_pa()) == 0)
        return 0;

      memset(pagetable, 0, PGSIZE);

      if (early_mode)
        *pte = PA2PTE(pagetable) | PTE_V;
      else
        *pte = PA2PTE(ADR2LOW(pagetable)) | PTE_V;
    }
  }
  return &pagetable[VPN_GET(va, 0)];
}
/**
 * walk_addr - Look up the physical address mapped to the va in the pagetable
 *
 * Context: Obtain a pointer to the page table entry (PTE) corresponding while
 * checking the validity of the page table
 *
 * Return: the PA of the found PTE
 */
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
      LOG_WARN("WARNING: no memory");
      return 0;
    }

    if (*pte & PTE_V)
      panic("mappages: remap");

    *pte = ((((uint64)pa) >> 12) << 10) | perm | PTE_V;
    if (a == last) {
      LOG_TRACE("va: %p pa: %p, perm: w: %d, r: %d, v:%d", (void *)a,
                (void *)pa, PTE_W & *pte, PTE_R & *pte, PTE_V & *pte);

      break;
    }

    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

/**
* kvmmap - Map physical memory to virtual memory
* @pagetable : Base address of the target pagetable
* @va : Virtual address
* @pa : Physical address
* @size : Memory size
* @perm : Permission
* 
* Context: Map physical memory to virtual memory
*
* Return: if success, return 1, otherwise return 0
*/
int kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm) {
  if (mappages(pagetable, va, pa, size, perm)) {
    return 1;
  }
  return 0;
}

// Ensure that the size is aligned to PGSIZE
void kvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free_pa) {
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

    if (do_free_pa) {
      kfree((void *)PTE2PA(*pte));
    }
    *pte = 0;
  }
}
