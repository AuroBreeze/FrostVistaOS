#include "core/proc.h"
#include "kernel/defs.h"
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

void check_signal(struct Process *proc)
{
	// SIGKILL/SIGSTOP can never be blocked or ignored (POSIX): handle them
	// before the blocked-mask filter, so a stale or foreign blocked bit can
	// never defer a kill. SIGKILL takes priority when both are pending.
	uint64 forced =
	    proc->sighand.sig_pending & ((1UL << SIGKILL) | (1UL << SIGSTOP));
	if (forced) {
		if (forced & (1UL << SIGKILL))
			sig_exit(SIGKILL);
		else
			sig_exit(SIGSTOP);
	}

	uint64 pending = proc->sighand.sig_pending;
	uint64 blocked = proc->sighand.sig_blocked;

	int ready = pending & ~blocked;
	while (ready) {
		int sig = lowest_set_bit(ready) + 1;
		ready &= (ready - 1);

		uint64 handler = proc->sighand.actions[sig].handler;
		if (handler == SIG_IGN) {
			proc->sighand.sig_pending &= ~(1UL << sig);
		} else if (handler == SIG_DFL) {
			proc->sighand.sig_pending &= ~(1UL << sig);
			sig_exit(sig);
		} else {
			// TODO(Phase 2): caught signal. Build a sigframe on the
			// user stack, point the trapframe at the handler, and
			// return to user mode. Until then the pending bit stays
			// set and the signal is re-checked on the next trap
			// return.
		}
	}
}
