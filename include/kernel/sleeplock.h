#ifndef __KERNEL_SLEEPLOCK_H__
#define __KERNEL_SLEEPLOCK_H__

#include "kernel/spinlock.h"

struct sleeplock {
	int locked;
	struct spinlock lock;

	char *name;
	int pid;
};

#endif
