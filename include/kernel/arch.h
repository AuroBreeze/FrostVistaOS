#ifndef __KERNEL_ARCH_H
#define __KERNEL_ARCH_H

// The active architecture include path supplies these headers.
#include "asm/context.h"
#include "asm/trapframe.h"

// Keep the common kernel independent of the concrete register layout.
typedef struct arch_context arch_context_t;
typedef struct arch_trapframe arch_trapframe_t;

#endif
