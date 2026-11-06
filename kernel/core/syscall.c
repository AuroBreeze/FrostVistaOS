
#define LOG_MODULE "SYSC"

#include "kernel/syscall.h"
#include "kernel/errno.h"
#include "asm/defs.h"
#include "kernel/arch/mm.h"
#include "kernel/proc.h"
#include "kernel/defs.h"
#include "kernel/string.h"
#include "kernel/log.h"
#include "kernel/macros.h"
#include "kernel/types.h"

// PERF: Copy the smaller of the page boundary or len, then search for \0 to
// avoid searching for a single character
int fetch_user_str(pagetable_t pagetable, char *dst, uint64 src_va,
		   uint64 max_len)
{
	char *buf = kalloc();
	if (!buf) {
		return -ENOMEM;
	}

	uint64 read_len = 0;
	uint64 reset_len = max_len;
	uint64 next_len = ARCH_PGSIZE - (src_va % ARCH_PGSIZE);

	while (reset_len > 0) {
		if (next_len > reset_len) {
			next_len = reset_len;
		}
		if (copyin(pagetable, buf, src_va + read_len, next_len) < 0) {
			kfree(buf);
			return -EFAULT;
		}

		for (int j = 0; j < next_len && (read_len + j) < max_len; j++) {
			if (!(dst[read_len + j] = buf[j])) {
				kfree(buf);
				return read_len + j;
			}
		}

		read_len += next_len;
		reset_len -= next_len;
		next_len = ARCH_PGSIZE;
	}

	kfree(buf);
	return -ENAMETOOLONG;
}

// xv6
// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
static int fetchstr(uint64 addr, char *buf, int max)
{
	struct Process *p = get_proc();
	return fetch_user_str(p->pagetable, buf, addr, max);
}

// xv6
static uint64 argraw(int n)
{
	struct Process *p = get_proc();
	switch (n) {
	case 0:
		return p->trapframe->a0;
	case 1:
		return p->trapframe->a1;
	case 2:
		return p->trapframe->a2;
	case 3:
		return p->trapframe->a3;
	case 4:
		return p->trapframe->a4;
	case 5:
		return p->trapframe->a5;
	default:
		return 0;
	}
	panic("argraw");
	return -1;
}

// xv6
// Fetch the nth 32-bit system call argument.
void argint(int n, int *ip)
{
	*ip = argraw(n);
}

// xv6
// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
void argaddr(int n, uint64 *ip)
{
	*ip = argraw(n);
}

// xv6
// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int argstr(int n, char *buf, int max)
{
	uint64 addr;
	argaddr(n, &addr);
	return fetchstr(addr, buf, max);
}

void syscall()
{
	struct Process *current_proc = get_proc();
	arch_trapframe_t *trapframe = current_proc->trapframe;

	uint64 num = trapframe->a7;
	if (num > 0 && num < ARRAY_SIZE(syscalls) && syscalls[num]) {
		LOG_TRACE("syscall %s", syscall_names[num]);
		uint64 ret = syscalls[num]();
		if (num != SYS_rt_sigreturn)
			trapframe->a0 = ret;
		LOG_TRACE("syscall %s: a0: 0x%x  done.", syscall_names[num],
			  trapframe->a0);
	} else {
		LOG_ERROR("Unknown syscall %d", num);
		trapframe->a0 = (uint64) -ENOSYS;
	}
}
