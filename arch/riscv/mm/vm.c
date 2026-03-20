#include "asm/defs.h"
#include "asm/machine.h"
#include "asm/mm.h"
#include "asm/riscv.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "other/tool.h"
#include "platform/PLIC.h"
#include "platform/uart.h"

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
  LOG_INFO("clear low memory mappings");
  // NOTE: Implemented a highly efficient method to clear the early low-address
  // identity mapping by simply nullifying the first entry of the root page
  // table (kernel_table[0] = 0;). In the RISC-V Sv39 architecture, the
  // top-level page table spans 512GB of virtual memory, with each of its 512
  // PTEs mapping a 1GB region. Consequently, kernel_table[0] corresponds
  // exclusively to the 0x00000000 - 0x3FFFFFFF range. Clearing this single
  // entry safely and elegantly unmaps the initial 1GB footprint, officially
  // completing the transition to a pure high-half kernel.

  for (int i = 0; i < 3; i++) {
    kernel_table[i] = 0;
  }
  sfence_vma();

  LOG_INFO("clear low memory mappings done");
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
  LOG_TRACE("%p", _divide);

  memset(pagetable, 0, PGSIZE);

  kvmmap(pagetable, (UART0_BASE), UART0_BASE, PGSIZE, PTE_R | PTE_W);
  kvmmap(pagetable, (PLIC_BASE), PLIC_BASE, PLIC_MM_SIZE,
         PTE_R | PTE_W | PTE_X);

  kvmmap(pagetable, KERNEL_BASE_LOW, KERNEL_BASE_LOW,
         (uint64)(_divide)-KERNEL_BASE_LOW, PTE_R | PTE_X);
  kvmmap(pagetable, (uint64)_divide, (uint64)_divide,
         PHYSTOP_LOW - (uint64)_divide, PTE_R | PTE_W);

  // Hight Address mapping
  LOG_TRACE("mapping high addresses");
  kvmmap(pagetable, PA2VA(UART0_BASE), UART0_BASE, PGSIZE, PTE_R | PTE_W);
  kvmmap(pagetable, KERNEL_BASE_HIGH, KERNEL_BASE_LOW,
         // NOTE: The address _divide here is not a high address,
         // because at this point it is merely mapped and has not
         // yet been jumped to via SP to load _divide
         PA2VA((uint64)_divide) - KERNEL_BASE_HIGH, PTE_R | PTE_X);

  kvmmap(pagetable, PA2VA((uint64)_divide), (uint64)_divide,
         PHYSTOP_HIGH - PA2VA((uint64)_divide), PTE_R | PTE_W);

  LOG_TRACE("\nmapping high addresses:\nhigh va: %p to pa: %p\nsize: %x",
            KERNEL_BASE_HIGH, KERNEL_BASE_LOW,
            PA2VA((uint64)_divide) - KERNEL_BASE_HIGH);

  LOG_TRACE("\nmapping high addresses:\nhigh va: %p to pa: %p\nsize: %x",
            (void *)PA2VA((uint64)_divide), (void *)_divide,
            PHYSTOP_HIGH - PA2VA((uint64)_divide));

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
      pagetable = early_mode ? (pagetable_t)pa : (pagetable_t)PA2VA(pa);
    } else {
      if (!alloc || (pagetable = (pte_t *)pt_alloc_page_pa()) == 0)
        return 0;

      memset(pagetable, 0, PGSIZE);

      if (early_mode)
        *pte = PA2PTE(pagetable) | PTE_V;
      else
        *pte = PA2PTE(VA2PA(pagetable)) | PTE_V;
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

  uint64 pa;
  pa = PTE2PA(*pte);
  return pa;
}

/**
 * mappages - Map physical memory to virtual memory
 * @pagetable : Base address of the target pagetable
 * @va : Virtual address
 * @pa : Physical address
 * @size : Memory size
 * @perm : Permission
 *
 * Context: NOTE the PA must be a real physical address
 *
 * Return: if success, return 1, otherwise return 0
 */
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

    if (*pte & PTE_V) {
      LOG_ERROR("pagetable: %p, va: %p, pa: %p", (void *)pagetable, (void *)a,
                (void *)pa);
      panic("mappages: remap");
    }

    *pte = ((((uint64)pa) >> 12) << 10) | perm | PTE_V;
    if (a == last) {
      LOG_TRACE("va: %p pa: %p, perm: r: %d, w: %d, x: %d, u: %d", (void *)a,
                (void *)pa, PTE_R & *pte, PTE_W & *pte, PTE_X & *pte,
                PTE_U & *pte);

      break;
    }

    a += PGSIZE;
    pa += PGSIZE;
  }
  return 1;
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
      continue;
      // panic("kvmunmap: walk failed");
    }
    if ((*pte & PTE_V) == 0) {
      continue;
      // panic("kvmunmap: not mapped");
    }
    if (PTE_FLAGS(*pte) == PTE_V) {
      continue;
      // panic("kvmunmap: not a leaf");
    }

    if (do_free_pa) {
      kfree((void *)PA2VA(PTE2PA(*pte)));
    }
    *pte = 0;
  }
}

/**
 * uvmunmap - Unmap a page table
 * @pagetable : Base address of the target pagetable
 * @va : Virtual address
 * @npage : Number of pages to unmap
 * @do_free : Whether to free the physical memory
 *
 * Return: void
 */
void uvmunmap(pagetable_t pagetable, uint64 va, int npage, int do_free) {
  uint64 a;
  pte_t *pte;

  for (a = va; a < va + npage * PGSIZE; a += PGSIZE) {
    if ((pte = walk(pagetable, a, 0)) == 0) {
      continue;
    }
    if ((*pte & PTE_V) == 0) {
      continue;
    }
    if (PTE_FLAGS(*pte) == PTE_V) {
      continue;
    }
    if (do_free) {
      kfree((void *)PA2VA(PTE2PA(*pte)));
    }
    *pte = 0;
  }
}

/**
 * uvmdealloc - Deallocate a region of memory
 * @pagetable : Base address of the target pagetable
 * @va : Virtual address must be aligned to PGSIZE
 * @size : Memory siz must be aligned to PGSIZE
 *
 * Context: This will delete an area of size `va` and free the memory.
 *
 * Return: 1
 */
uint64 uvmdealloc(pagetable_t pagetable, uint64 va, uint64 size) {
  LOG_TRACE("uvmdealloc: va: %p, size: %d", (void *)va, size);
  if (va % PGSIZE != 0 || size % PGSIZE != 0) {
    panic("uvmdealloc: va/size not aligned");
  }
  uvmunmap(pagetable, va, size / PGSIZE, 1);
  LOG_TRACE("uvmdealloc: success");
  return 1;
}

/**
 * uvmalloc - Automatically acquire spatial data and map it
 * @pagetable : Base address of the target pagetable
 * @va : Virtual address
 * @size : Memory size
 * @perm : Permission
 *
 * Context: Will assign the size of the corresponding VA mapping,
 *
 * Return: if success, return 1, otherwise return 0
 * */
uint64 uvmalloc(pagetable_t pagetable, uint64 va, uint64 size, int perm) {
  LOG_TRACE("uvmalloc: va: %p, size: %d, perm: %d", (void *)va, size, perm);
  uint64 start = PGROUNDDOWN(va);
  uint64 end = PGROUNDUP(va + size);

  for (uint64 i = start; i < end; i += PGSIZE) {
    char *mem = kalloc();
    if (mem == 0) {
      LOG_WARN("uvmalloc: memory allocation failed");
      uvmdealloc(pagetable, start, i - start);
      return 0;
    }

    if (mappages(pagetable, i, (uint64)VA2PA(mem), PGSIZE,
                 perm | PTE_U | PTE_V) == 0) {
      LOG_WARN("uvmalloc: mappages failed");
      kfree(mem);
      uvmdealloc(pagetable, start, i - start);
      return 0;
    }
  }
  LOG_TRACE("uvmalloc: success");
  return 1;
}

/**
 * freewalk - Free the page table and the physical memory
 *
 * Context: This releases the space occupied by the page table, not the memory
 * locations it maps.
 *
 * Return: void
 */
void freewalk(pagetable_t pagetable) {
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
      uint64 child_pa = PTE2PA(pte);
      freewalk((pagetable_t)PA2VA(child_pa));
      pagetable[i] = 0;
    } else if (pte & PTE_V) {
      LOG_TRACE("freewalk: leaf node %p", (void *)pagetable);
      LOG_WARN("freewalk: leaf node still exists!");
    }
  }
  kfree((void *)pagetable);
}

/**
 * uvmfree - Completely clear the page table and all the space it occupies
 *
 * Return: void
 */
void uvmfree(pagetable_t pagetable, uint64 size) {
  if (size > 0) {
    uint64 npage = PGROUNDUP(size) / PGSIZE;
    uvmunmap(pagetable, 0, npage, 1);
  }

  for (int i = 256; i < 512; i++) {
    pagetable[i] = 0;
  }

  freewalk(pagetable);
}

/**
 * uvmcopy - Copy memory from old to new
 *
 * @old : Base address of the source pagetable
 * @new : Base address of the target pagetable
 *
 * Context: Used to copy memory from one page table to another
 *
 * Return: if success, return 1, otherwise return 0
 */
int uvmcopy(pagetable_t old, pagetable_t new) {
  LOG_TRACE("uvmcopy: old: %p, new: %p", (void *)old, (void *)new);
  pte_t *pte2, *pte1, *pte0;
  uint64 pa, va;
  uint64 flags;
  char *mem;

  for (int i2 = 0; i2 < 256; i2++) {
    pte2 = &old[i2];
    if (!(*pte2 & PTE_V)) {
      continue;
    }
    pagetable_t pt1 = (pagetable_t)PA2VA(PTE2PA(*pte2));

    for (int i1 = 0; i1 < 512; i1++) {
      pte1 = &pt1[i1];
      if (!(*pte1 & PTE_V)) {
        continue;
      }
      pagetable_t pt0 = (pagetable_t)PA2VA(PTE2PA(*pte1));

      for (int i0 = 0; i0 < 512; i0++) {
        pte0 = &pt0[i0];
        if (!(*pte0 & PTE_V)) {
          continue;
        }

        va = ((uint64)i2 << 30 | (uint64)i1 << 21 | (uint64)i0 << 12);
        pa = PTE2PA(*pte0);
        flags = PTE_FLAGS(*pte0);

        if ((mem = kalloc()) == 0) {
          goto err;
        }
        memmove(mem, (void *)PA2VA(pa), PGSIZE);
        if (!mappages(new, va, (uint64)VA2PA(mem), PGSIZE, flags)) {
          kfree(mem);
          goto err;
        }
      }
    }
  }

  LOG_TRACE("uvmcopy: success");
  return 1;
err:
  LOG_TRACE("uvmcopy: failed");
  return 0;
}

int copyin(pagetable_t pagetabel, char *dst, uint64 src, int len) {
  while (len > 0) {
    uint64 va = PGROUNDDOWN((uint64)src);
    uint64 pa = (walk_addr(pagetabel, va));
    if (pa == 0) {
      LOG_WARN("copyin: walk_addr failed");
      return 0;
    }
    uint64 kernel_va = PA2VA(pa);

    uint64 offset = src - va;
    int size = PGSIZE - offset;

    if (size > len) {
      size = len;
    }
    memmove((void *)dst, (void *)(kernel_va + offset), size);

    len -= size;
    dst += size;
    src += size;
  }
  return 1;
}
