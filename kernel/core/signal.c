#include "asm/defs.h"
#include "asm/signal.h"
#include "kernel/proc.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/signal.h"

/**
 * signal_reset_on_exec - Reset signal state across exec
 *
 * Handlers and pending signals belong to the old program image: handlers are
 * function pointers into the old address space (dangling after exec), and
 * pending signals are stale events from the previous lifecycle. The blocked
 * mask is process state, not program state, so it is preserved (POSIX:
 * "Blocked signals shall remain blocked").
 *
 * Context: exec, after the new address space is committed. p->lock guards
 * sig_pending against a concurrent kill() on another CPU.
 */
void signal_reset_on_exec(struct Process *p)
{
	acquire(&p->lock);

	p->sighand.sig_pending = 0;

	// Caught signals fall back to SIG_DFL; ignored signals stay ignored
	// (POSIX). Signal 0 is unused, so the scan starts at 1.
	for (int i = 1; i < NSIG; i++) {
		if (p->sighand.actions[i].handler != SIG_IGN) {
			p->sighand.actions[i].handler = SIG_DFL;
			p->sighand.actions[i].flags = 0;
			p->sighand.actions[i].restorer = 0;
			p->sighand.actions[i].mask = 0;
		}
	}

	release(&p->lock);
}

/**
 * sig_exit - Exit from a signal handler
 *
 * Lock Contact:
 *  Entry: must not hold proc->lock
 *
 * sig_exit will call exit(), which will panic if it holds proc->lock
 * */
static void sig_exit(int sig)
{
	struct Process *p = get_proc();

	exit(128 + sig); // Convention: Exit code for death due to a signal =
			 // 128 + signal number (shell convention)
}

/**
 * lowest_set_bit - Index of the lowest set bit in a mask
 *
 * RISC-V base ISA has no ctz instruction, and the freestanding kernel does
 * not link libgcc, so __builtin_ctzll() would emit an undefined reference
 * to __ctzdi2. This pure-C binary search replaces it.
 *
 * Precondition: mask != 0.
 */
static int lowest_set_bit(uint64 mask)
{
	int n = 0;
	if ((mask & 0xFFFFFFFFULL) == 0) {
		mask >>= 32;
		n += 32;
	}
	if ((mask & 0xFFFFULL) == 0) {
		mask >>= 16;
		n += 16;
	}
	if ((mask & 0xFFULL) == 0) {
		mask >>= 8;
		n += 8;
	}
	if ((mask & 0xFULL) == 0) {
		mask >>= 4;
		n += 4;
	}
	if ((mask & 0x3ULL) == 0) {
		mask >>= 2;
		n += 2;
	}
	if ((mask & 0x1ULL) == 0)
		n += 1;
	return n;
}

int signal_pending(struct Process *p)
{
	return (p->sighand.sig_pending & ~p->sighand.sig_blocked) != 0;
}

/**
 * signal_send - Send a signal to a process
 *
 * Lock Contact:
 *  Entry: must not hold proc->lock
 *  Exit: will hold proc->lock
 * */
void signal_send(struct Process *p, int sig)
{

	p->sighand.sig_pending |= SIGMASK(sig);

	release(&p->lock);
	if (p->state == SLEEPING)
		wakeup(p);
	acquire(&p->lock);
}

void check_signal(struct Process *proc)
{
	// SIGKILL/SIGSTOP can never be blocked or ignored (POSIX): handle them
	// before the blocked-mask filter, so a stale or foreign blocked bit can
	// never defer a kill. SIGKILL takes priority when both are pending.
	acquire(&proc->lock);
	uint64 forced =
	    proc->sighand.sig_pending & (SIGMASK(SIGKILL) | SIGMASK(SIGSTOP));
	if (forced) {
		if (forced & SIGMASK(SIGKILL)) {
			release(&proc->lock);
			sig_exit(SIGKILL);
		} else {
			release(&proc->lock);
			sig_exit(SIGSTOP);
		}
	}

	uint64 pending = proc->sighand.sig_pending;
	uint64 blocked = proc->sighand.sig_blocked;

	uint64 ready = pending & ~blocked;
	while (ready) {
		int sig = lowest_set_bit(ready) + 1;
		ready &= (ready - 1);

		uint64 handler = proc->sighand.actions[sig].handler;
		if (handler == SIG_IGN) {
			proc->sighand.sig_pending &= ~SIGMASK(sig);
		} else if (handler == SIG_DFL) {
			proc->sighand.sig_pending &= ~SIGMASK(sig);
			release(&proc->lock);
			sig_exit(sig);
		} else {
			struct sigaction action = proc->sighand.actions[sig];
			if (action.restorer == 0) {
				release(&proc->lock);
				LOG_WARN("signal %d has no restorer", sig);
				return;
			}

			uint64 old_sp = proc->trapframe->sp;
			uint64 frame_sp =
			    (old_sp - sizeof(struct sigframe)) & ~0xF;
			struct sigframe frame;
			frame.saved_tf = *proc->trapframe;
			frame.saved_mask = proc->sighand.sig_blocked;
			if (copyout(proc->pagetable, (char *) frame_sp,
				    (uint64) &frame, sizeof(frame)) < 0) {
				release(&proc->lock);
				LOG_WARN("copyout failed");
				return;
			}

			proc->sighand.sig_pending &= ~SIGMASK(sig);
			proc->trapframe->sp = frame_sp;
			proc->trapframe->arch_epc = handler;
			proc->trapframe->a0 = sig;
			proc->sighand.sig_blocked |= action.mask | SIGMASK(sig);
			proc->trapframe->ra = action.restorer;
		}
	}
	release(&proc->lock);
}
