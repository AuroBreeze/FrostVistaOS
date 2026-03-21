#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include "kernel/types.h"

struct spinlock {
  uint locked;
  char *name;
  struct cpu *cpu;
};

#endif
