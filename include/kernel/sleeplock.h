#ifndef FV_KERNEL_SLEEPLOCK_H
#define FV_KERNEL_SLEEPLOCK_H

#include "kernel/spinlock.h"

struct sleeplock {
	int locked;
	struct spinlock lock;

	char *name;
	int pid;
};

#endif
