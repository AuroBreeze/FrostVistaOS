#ifndef __ASM_SYSCALL_H__
#define __ASM_SYSCALL_H__

#define SYS_write 1

#include "kernel/defs.h"
void syscall(uint64 *regs);

#endif
