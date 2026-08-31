#include "asm/trap.h"
#include "asm/irq.h"
#include "asm/loongarch.h"
#include "asm/mm.h"
#include "kernel/arch/trap.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/proc.h"
#include "kernel/types.h"

#define ESTAT_IS_TIMER (1ULL << 11)

void usertrapret(void);

/*
 * LoongArch currently enters kernelvec with the current SP directly.  There
 * is no RISC-V-style sscratch stack exchange in this trap path, so the
 * architecture hook intentionally has no register operation yet.
 */
void set_kernel_stack(uint64 stack_top)
{
	// 默认 save0 保存内核栈地址
	w_kscratch0(stack_top);
}

static __attribute__((noreturn)) void trap_halt(void)
{
	for (;;) {
		asm volatile("idle 0");
	}
}

void kerneltrap(void)
{
	uint64 estat = r_estat();
	uint64 era = r_era();
	uint64 badv = r_badv();

	if (is_interrupt(estat)) {
		uint64 is = estat_is(estat);
		if (is & ESTAT_IS_TIMER) {
			w_ticlr(1);
			timerintr();
			LOG_TRACE("Kernel timer interrupt");
			return;
		}

		LOG_ERROR("Unhandled kernel interrupt: is=0x%x era=%p", is,
			  (void *) era);
		trap_halt();
	}

	uint64 ecode = estat_ecode(estat);
	uint64 esubcode = estat_esubcode(estat);
	switch (ecode) {
	case LA_ECODE_PIL:
		LOG_ERROR("Kernel load page invalid");
		break;
	case LA_ECODE_PIS:
		LOG_ERROR("Kernel store page invalid");
		break;
	case LA_ECODE_PIF:
		LOG_ERROR("Kernel instruction fetch page invalid");
		break;
	case LA_ECODE_PME:
		LOG_ERROR("Kernel page modification exception");
		break;
	case LA_ECODE_PNR:
		LOG_ERROR("Kernel page non-readable");
		break;
	case LA_ECODE_PNX:
		LOG_ERROR("Kernel page non-executable");
		break;
	case LA_ECODE_PPI:
		LOG_ERROR("Kernel page privilege illegal");
		break;
	case LA_ECODE_SYS:
		LOG_ERROR("Unexpected kernel system call");
		break;
	case LA_ECODE_BRK:
		LOG_ERROR("Kernel breakpoint");
		break;
	case LA_ECODE_INE:
		LOG_ERROR("Kernel undefined instruction");
		break;
	case LA_ECODE_IPE:
		LOG_ERROR("Kernel instruction privilege error");
		break;
	default:
		LOG_ERROR("Unknown kernel exception");
		break;
	}
	LOG_ERROR("Unhandled kernel exception: ecode=0x%x esubcode=0x%x "
		  "era=%p badv=%p",
		  ecode, esubcode, (void *) era, (void *) badv);

	/* 未实现的异常不能直接 ertn，否则会重新执行同一条故障指令。 */
	trap_halt();
}

void usertrap(void)
{
	uint64 sp = r_sp();
	arch_trapframe_t *tf =
	    (arch_trapframe_t *) (PGROUNDUP(sp) - sizeof(arch_trapframe_t));

	struct Process *p = get_proc();
	p->trapframe = tf;
	tf->arch_epc = r_era();

	uint64 estat = r_estat();
	uint64 ecode = estat_ecode(estat);
	if (is_interrupt(estat)) {
		uint64 is = estat_is(estat);
		if (is & ESTAT_IS_TIMER) {
			w_ticlr(1);
			timerintr();
			LOG_TRACE("Timer PLV3");
			yield();
			goto out;
		}

		LOG_ERROR("Unexpected user interrupt: is=0x%x", is);
		goto fault;
	}

	if (ecode == LA_ECODE_SYS) {
		LOG_TRACE("SYSCALL from PLV3");
		tf->arch_epc += 4;
		syscall();
		yield();
		goto out;
	}

	if (estat_is_page_fault(estat))
		LOG_WARN("User page fault: ecode=%d badv=%p", ecode, r_badv());

fault:
	LOG_ERROR("Unhandled user trap: ecode=%d esubcode=%d", ecode,
		  estat_esubcode(estat));
	trap_halt();

out:
	usertrapret();
}

void usertrapret(void)
{
	struct Process *p = get_proc();

	irq_disable();
	if (holding(&p->lock)) {
		release(&p->lock);
		irq_disable();
	}

	check_signal(p);

	uint64 prmd = r_prmd();
	prmd &= ~PRMD_PPLV_MASK;
	prmd |= PRMD_PPLV_PLV3 | PRMD_PIE;
	w_prmd(prmd);
	w_era(p->trapframe->arch_epc);

	extern void userret(arch_trapframe_t *);
	userret(p->trapframe);
}

void trap_handle(void)
{
	LOG_TRACE("Enter trap handler");
	uint64 prmd = r_prmd();
	uint64 pplv = prmd & PRMD_PPLV_MASK;
	if (pplv == PRMD_PPLV_PLV0) {
		LOG_TRACE("PLV0 tarp handle");
		kerneltrap();
		return;
	} else if (pplv == PRMD_PPLV_PLV3) {
		LOG_TRACE("PLV3 tarp handle");
		usertrap();
		return;
	}

	LOG_ERROR("PPLV: 0x%x\n", pplv);
	panic("Unknown PPLV");
}

void arch_trap_handle(void)
{
	trap_handle();
}

void arch_usertrapret(void)
{
	usertrapret();
}
