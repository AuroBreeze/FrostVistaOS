#include "kernel/kalloc.h"
#include "kernel/machine.h"
#include "kernel/printf.h"
#include "kernel/string.h"
#include "kernel/types.h"

static void freerange(void *pa_start, void *pa_end);
// the starting position of free space in the kernel stack,
// define in LD
extern char _kernel_end[];

// use a linked list to store free memory
struct IdleMM {
  struct IdleMM *next;
};

int cnt = 0;

struct freeMemory {
  struct IdleMM *freelist;
} FMM;

void kalloc_init() {
  kprintf("kalloc_init start\n");
  freerange(_kernel_end, (void *)PHYSTOP);
  kprintf("Total Memory Pages: %d\n", cnt);
}

static void freerange(void *pa_start, void *pa_end) {
  char *ps = (char *)pa_start;
  char *pe = (char *)pa_end;
  for (; ps + PGSIZE <= (char *)pe; ps += 4096) {
    kfree((void *)ps);
  }
}

void kfree(void *pa) {

  uint64 p = (uint64)pa;
  if ((p % PGSIZE != 0) || (p > (uint64)PHYSTOP) || (p < _kernel_end))
    panic("kfree encounter an error");

  struct IdleMM *M;
  memset(pa, 0, PGSIZE);

  M = (struct IdleMM *)pa;

  M->next = FMM.freelist;
  FMM.freelist = M;
  cnt++;
}
