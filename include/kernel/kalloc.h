#ifndef KALLOC_H
#define KALLOC_H

#include "kernel/types.h"
// use a linked list to store free memory

struct IdleMM {
  struct IdleMM *next;
};

struct freeMemory {
  struct IdleMM *freelist;
  uint64 size;
};

// need to be initialized in kalloc.c
extern struct freeMemory FMM;
extern int cnt;
extern struct IdleMM head;
extern char *ekalloc_ptr;

// the starting position of free space in the kernel stack,
// define in LD
extern char _kernel_end[];

#endif
