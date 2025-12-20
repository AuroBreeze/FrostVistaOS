#include "kernel/kalloc.h"
#include "kernel/defs.h"
#include "kernel/machine.h"
#include "kernel/types.h"

// #define DEBUG
static void freerange(void *pa_start, void *pa_end);

// Initialization
struct freeMemory FMM;
struct IdleMM head;
int cnt = 0;
char *ptr = (char *)_kernel_end;

// Enable sv39 paging and high address mapping
void kalloc_init() {
#ifdef DEBUG
  kprintf("kalloc_init start\n");
#endif
  FMM.freelist = &head;
  head.next = FMM.freelist;
  freerange((void *)ADR2HIGHT(((uint64)(ptr) + EPAGES * PGSIZE)),
            (void *)ADR2HIGHT(PHYSTOP));
#ifdef DEBUG
  kprintf("Total Memory Pages: %d\n", cnt);
#endif
}

// enable sv39 paging and high address mapping
// Mount all released memory to the virtul high address range
static void freerange(void *pa_start, void *pa_end) {
  if (IS_ADR_LOW(pa_start) || IS_ADR_LOW(pa_end)) {
    kprintf("pa: %p\npe: %p\n", pa_start, pa_end);
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
void kfree(void *pa) {
  uint64 p = (uint64)pa;
  uint64 kva = (uint64)pa;

  if (!IS_ADR_HIGHT(p)) {
    kprintf("pa: %p\n", p);
    panic("kfree: Low-address space cannot be released\n");
  }
  // High Address to Low Address
  p = ADR2LOW(p);

  if ((p % PGSIZE != 0) || (p > (uint64)PHYSTOP) || (p < (uint64)_kernel_end))
    panic("kfree encounter an error");
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
  if (((uint64)ptr % PGSIZE) != 0)
    panic("ekalloc panic\n");

  void *ret = ptr;
  ptr += PGSIZE;
  return ret;
}
