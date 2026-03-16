#ifndef __ASM_SYSCALL_H__
#define __ASM_SYSCALL_H__

#define SYS_write 1

struct trapframe;
void syscall(struct trapframe *tf);

#endif
