#include "kernel/defs.h"
#include "kernel/machine.h"
#include "kernel/types.h"

static void freerange(void *pa_start, void *pa_end);
// the starting position of free space in the kernel stack,
// define in LD
extern char _kernel_end[];

// use a linked list to store free memory
struct IdleMM {
  struct IdleMM *next;
};

static int cnt = 0;

struct freeMemory {
  struct IdleMM *freelist;
} FMM;

static struct IdleMM head;

void kalloc_init() {
#ifdef DEBUG
  kprintf("kalloc_init start\n");
#endif
  FMM.freelist = &head;
  head.next = FMM.freelist;
  freerange(_kernel_end, (void *)PHYSTOP);
#ifdef DEBUG
  kprintf("Total Memory Pages: %d\n", cnt);
#endif
}

static void freerange(void *pa_start, void *pa_end) {
  char *ps = (char *)pa_start;
  char *pe = (char *)pa_end;
  for (; ps + PGSIZE <= pe; ps += 4096) {
    kfree((void *)ps);
  }
}

void kfree(void *pa) {

  uint64 p = (uint64)pa;
  if ((p % PGSIZE != 0) || (p > (uint64)PHYSTOP) || (p < (uint64)_kernel_end))
    panic("kfree encounter an error");

  struct IdleMM *M;
  memset(pa, 0, PGSIZE);

  M = (struct IdleMM *)pa;

  M->next = head.next;
  head.next = M;
  cnt++;
}

void *kalloc() {
  if (head.next == &head) {
    return 0;
  }

  struct IdleMM *temp = head.next;
  head.next = temp->next;

  memset(temp, 0, PGSIZE);

  return (void *)temp;
}
