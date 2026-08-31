
#define LOG_MODULE "TRAP"

#include "asm/trap.h"
#include "asm/defs.h"
#include "asm/mm.h"
#include "asm/riscv.h"
#include "kernel/proc.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "other/tool.h"
#include "platform/board.h"
#include "platform/timer.h"
#include "platform/virtio_mmio.h"

// define the kernelvec function in assembly
extern void kernelvec(void);
void trapinit()
{
	LOG_TRACE("trapinit: kernelvec: %p", kernelvec);
	w_stvec((uint64) kernelvec);
}

void set_kernel_stack(uint64 stack_top)
{
	w_sscratch(stack_top);
}

void s_trap_handler(void)
{
	uint64 sc = r_scause();
	uint64 epc = r_sepc();
	uint64 tval = r_stval();

	if ((sc >> 63) == 1) {
		uint64 cause = sc & ((1ULL << 63) - 1);
		// determine if it's a timer interrupt
		// to notify S state by setting SIP_SSIP (Software Interrupt
		// Pending)
		if (cause == E_S_TIMER_INTERRUPT) {
			sbi_set_timer(r_time() + 1000000);
			LOG_TRACE("Tick s_trap_handler");
			timerintr();
			return;
		}
		if (cause == E_S_EXTERNAL_INTERRUPT) {
			int id = cpuid();
			int context = (2 * id) + 1;

			int irq = plic_claim_interrupt(context);

			if (irq == 0) {
				LOG_DEBUG("SEI: spurious irq\n");
				return;
			}

			if (irq == UART_IRQ) {
				uartintr();
			} else if (irq == VIRTIO_IRQ) {
				virtio_disk_intr();
			} else if (irq != 0) {
				LOG_ERROR("SEI: unexpected irq=%d\n", irq);
			}
			if (irq) {
				plic_complete_interrupt(context, irq);
			}

			return;
		}
	}

	LOG_ERROR("=== TRAP ===");
	LOG_ERROR("Interrupt: %d, Exception: %d", (sc >> 63) == 1,
		  (sc >> 63) == 0);
	LOG_ERROR("scause=%p sepc=%p stval=%p", (void *) sc, (void *) epc,
		  (void *) tval);

	// Simple Tips
	if ((sc >> 63) == 0) {
		switch (sc) {
		case I_S_ILLEGAL_INSTRUCTION:
			LOG_ERROR("cause: illegal instruction");
			break;
		case I_S_BREAKPOINT:
			LOG_ERROR("cause: breakpoint");
			epc = next_pc(epc);
			w_sepc(epc);
			return;
		case I_S_INSTRUCTION_PAGE_FAULT:
			LOG_ERROR("cause: instruction page fault");
			break;
		case I_S_LOAD_PAGE_FAULT:
			LOG_ERROR("cause: load page fault");
			break;
		case I_S_STORE_PAGE_FAULT:
			LOG_ERROR("cause: store/amo page fault");
			break;
		default:
			break;
		}
	}

	panic("panic trap");
}

void arch_trap_handle(void)
{
	s_trap_handler();
}

void usertrapret(void)
{
	// LOG_TRACE("usertrapret");
	// Set SIP that turns off all interrupts
	irq_disable();

	// release proc_lock
	struct Process *p = get_proc();
	if (holding(&p->lock)) {
		release(&p->lock);
		irq_disable();
	}

	// check pending signals
	check_signal(p);

	// write kernel trap vector
	extern void uservec(void);
	w_stvec((uint64) uservec);

	// set S Previous Privilege mode to User.
	unsigned long x = r_sstatus();
	x &= ~SSTATUS_U_SPP; // clear SPP to 0 for user mode
	x |= SSTATUS_SPIE;   // enable interrupts in user mode
	w_sstatus(x);

	w_sepc(p->trapframe->arch_epc);

	extern void userret(arch_trapframe_t *);
	userret(p->trapframe);
}

void arch_usertrapret(void)
{
	usertrapret();
}

void usertrap(void)
{
	if ((r_sstatus() & SSTATUS_U_SPP) != 0) {
		panic("usertrap: not from user mode");
	}
	// write kernel trap vector that handles new interrupts in S mode
	trapinit();

	uint64 sp = r_sp();
	arch_trapframe_t *tf =
	    (arch_trapframe_t *) (PGROUNDUP(sp) - sizeof(arch_trapframe_t));

	struct Process *p = get_proc();
	p->trapframe = tf;

	tf->arch_epc = r_sepc();
	uint64 cause = r_scause();

	if ((cause >> 63) == 1) {
		uint64 exception_code = cause & ((1ULL << 63) - 1);
		if (exception_code == E_S_TIMER_INTERRUPT) {
			sbi_set_timer(r_time() + 1000000);
			LOG_TRACE("Tick usertrap");
			timerintr();
			yield();
		} else if (exception_code == E_S_EXTERNAL_INTERRUPT) {
			int id = cpuid();
			int context = (2 * id) + 1;
			int irq = plic_claim_interrupt(context);

			if (irq == UART_IRQ) {
				uartintr();
			} else if (irq == VIRTIO_IRQ) {
				virtio_disk_intr();
			} else if (irq != 0) {
				LOG_ERROR("U-mode SEI: unexpected irq=%d\n",
					  irq);
			}
			if (irq) {
				plic_complete_interrupt(context, irq);
			}
		} else {
			LOG_ERROR("Unexpected interrupt in U-mode, code: %d",
				  exception_code);
		}
	} else {
		if (cause == 8) {
			LOG_TRACE("Target Eliminated: Successfully executed "
				  "'ecall' in U-mode!");
			tf->arch_epc += 4;
			syscall();

			yield();
		} else if (cause == I_S_INSTRUCTION_PAGE_FAULT ||
			   cause == I_S_LOAD_PAGE_FAULT ||
			   cause == I_S_STORE_PAGE_FAULT) {
			uint64 tval = r_stval();
			struct Process *current_proc = get_proc();

			if (cause == I_S_STORE_PAGE_FAULT &&
			    is_cow_fault(current_proc->pagetable, tval) == 0) {
				if (handle_cow_fault(current_proc->pagetable,
						     tval) == 0)
					goto end;
			}

			if (cause != I_S_INSTRUCTION_PAGE_FAULT && tval != 0 &&
			    current_proc->heap_bottom <= tval &&
			    tval < current_proc->heap_top) {
				if (handle_page_fault(current_proc->pagetable,
						      tval) == 0)
					goto end;
			} else if (tval > current_proc->heap_top &&
				   tval < current_proc->stack_bottom &&
				   find_overlapping_vma(tval, 1) != 0) {
				if (handle_vma_fault(tval) == 0)
					goto end;
			}

			LOG_WARN("unrecoverable user page fault: cause=%d "
				 "tval=%p pid=%d",
				 cause, (void *) tval, current_proc->pid);
			acquire(&current_proc->lock);
			current_proc->sighand.sig_pending |= SIGMASK(SIGSEGV);
			release(&current_proc->lock);
			goto end;

		} else {
			LOG_ERROR("Unexpected trap, cause: %d", cause);
			while (1)
				;
		}
	}

end:
	usertrapret();
}
