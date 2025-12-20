#include "kernel/kalloc.h"
#include "kernel/defs.h"
#include "kernel/machine.h"
#include "kernel/types.h"

// #define DEBUG
static void freerange(void *pa_start, void *pa_end);
static void efreerange(void *pa_start, void *pa_end);

// Initialization
struct freeMemory FMM, EFMM;
struct IdleMM head;
struct IdleMM ehead;
int cnt = 0;

// Enable sv39 paging and high address mapping
void kalloc_init() {
#ifdef DEBUG
  kprintf("kalloc_init start\n");
#endif
  FMM.freelist = &head;
  head.next = FMM.freelist;
  freerange((void *)ADR2HIGHT(((uint64)(_kernel_end) + EPAGES * PGSIZE)),
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
  cnt++;
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

  cnt--;
  FMM.size--;

  memset(temp, 0, PGSIZE);

  return (void *)(temp);
}

// Pagination: Before Enabling
void ekalloc_init() {
#ifdef DEBUG
  kprintf("ekalloc_init start\n");
#endif
  EFMM.freelist = &ehead;
  ehead.next = EFMM.freelist;
  efreerange(_kernel_end, (void *)(_kernel_end + EPAGES * PGSIZE));
#ifdef DEBUG
  kprintf("eTotal Memory Pages: %d\n", cnt);
#endif
}

// Pagination: Before Enabling
static void efreerange(void *pa_start, void *pa_end) {

  char *ps = (char *)(pa_start);
  char *pe = (char *)(pa_end);
#ifdef DEBUG
  kprintf("ps: %p\npe: %p\n", ps, pe);
#endif
  for (; ps + PGSIZE <= pe; ps += PGSIZE) {
    ekfree((void *)ps);
  }
}

// Pagination: Before Enabling
void ekfree(void *pa) {
  uint64 p = (uint64)pa;

  // High Address to Low Address
  if ((p % PGSIZE != 0) || (p > (uint64)PHYSTOP) || (p < (uint64)_kernel_end))
    panic("ekfree encounter an error");

  struct IdleMM *M;
  M = (struct IdleMM *)p;
  M->next = ehead.next;
  ehead.next = M;
  cnt++;
  EFMM.size++;
}
// Pagination: Before Enabling
// Only allocate pages that are absolutely necessary
void *ekalloc() {
  if (ehead.next == &ehead) {
    return 0;
  }

  struct IdleMM *temp = ehead.next;
  ehead.next = temp->next;
  cnt--;
  EFMM.size--;
  memset(temp, 0, PGSIZE);

  return (void *)(temp);
}
