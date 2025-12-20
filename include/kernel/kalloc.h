#ifndef KALLOC_H
#define KALLOC_H

#include "kernel/types.h"
// use a linked list to store free memory
struct IdleMM {
  struct IdleMM *next;
};

int cnt = 0;

struct freeMemory {
  struct IdleMM *freelist;
  uint64 size;
} FMM, EFMM;

struct IdleMM head;
struct IdleMM ehead;

// the starting position of free space in the kernel stack,
// define in LD
extern char _kernel_end[];

#endif
