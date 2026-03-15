#include "kernel/kalloc.h"
#include "asm/machine.h"
#include "asm/mm.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"

static void freerange(void *pa_start, void *pa_end);

// Initialization
struct freeMemory FMM;
struct IdleMM head;
int cnt = 0;
char *ekalloc_ptr = (char *)_kernel_end;

// Enable sv39 paging and high address mapping
void kalloc_init() {
  LOG_INFO("kalloc_init start");
  FMM.freelist = &head;
  head.next = FMM.freelist;
  freerange((void *)ADR2HIGH((uint64)(ekalloc_ptr)), (void *)PHYSTOP_HIGH);

  LOG_INFO("Total Memory Pages: %d", FMM.size);
  LOG_INFO("kalloc_init end");
}

// enable sv39 paging and high address mapping
// Mount all released memory to the virtul high address range
static void freerange(void *pa_start, void *pa_end) {
  if (IS_ADR_LOW(pa_start) || IS_ADR_LOW(pa_end)) {
    LOG_ERROR("pa: %p\npe: %p\n", pa_start, pa_end);
    panic("freerange: It must be a high address");
  }
  char *ps = (char *)pa_start;
  char *pe = (char *)pa_end;
  for (; ps + PGSIZE <= pe; ps += PGSIZE) {
    kfree((void *)ps);
  }
}

// Enable sv39 paging and high address mapping
// pa must be a high address
// Moount virtul hight addresses after releasing memory
/**
 * kfree - Release a page
 * @va: the virtual address to free
 *
 * Context:
 *
 * Return: void
 */
// NOTE:  I have changed the PA here to VA, because it was previously written as
// PA due to using an identity mapping and not understanding address
// translation. Now it should be changed to VA to clarify the parameters that
// need to be filled in. There is no distinction between high and low for PA.
void kfree(void *va) {
  uint64 p = (uint64)va;
  uint64 kva = (uint64)va;

  if (!IS_ADR_HIGH(p)) {
    LOG_ERROR("pa: %p\n", p);
    panic("kfree: Low-address space cannot be released\n");
  }

  if ((p % PGSIZE != 0) || (p > PHYSTOP_HIGH) || (p < (uint64)_kernel_end)) {
    LOG_TRACE("PHYSTOP: %p\n", (uint64)PHYSTOP_LOW);
    LOG_TRACE("_kernel_end: %p\n", (uint64)_kernel_end);
    LOG_TRACE("align: %x   _kernel_end: %x   PHYSTOP: %x\n", p % PGSIZE != 0,
              p<(uint64)_kernel_end, p> PHYSTOP_HIGH);
    LOG_TRACE("va: %p  p: %p\n", (void *)va, (void *)p);

    panic("kfree encounter an error");
  }
  struct IdleMM *M;

  // kprintf("kva: %p\n", (void *)kva);
  memset((void *)kva, 0, PGSIZE);

  M = (struct IdleMM *)kva;

  M->next = head.next;
  head.next = M;
  FMM.size++;
}

// Enable sv39 paging and high address mapping
// return to high address
void *kalloc() {
  if (head.next == &head) {
    return 0;
  }

  struct IdleMM *temp = head.next;
  head.next = temp->next;

  FMM.size--;

  memset(temp, 0, PGSIZE);

  return (void *)(temp);
}

void *ekalloc(void) {
  if (((uint64)ekalloc_ptr % PGSIZE) != 0)
    panic("ekalloc panic\n");

  void *ret = ekalloc_ptr;
  ekalloc_ptr += PGSIZE;
  return ret;
}
