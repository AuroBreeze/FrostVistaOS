#ifndef FV_LOONGARCH_TRAP_H
#define FV_LOONGARCH_TRAP_H

#include "kernel/types.h"

// ECFG.VS occupies bits [18:16] and sets vector entry spacing to 2^VS
// instructions. VS is currently zero, so all exceptions and interrupts enter
// kernelvec.
#define ECFG_VS_SHIFT 16
#define ECFG_VS(x) ((x) << ECFG_VS_SHIFT)

// ESTAT.Ecode[21:16] stores the primary exception code,
// EsubCode[30:22] stores the secondary exception code, and IS[12:0] stores
// the pending interrupt state.
#define ESTAT_ECODE_SHIFT 16
#define ESTAT_ESUBCODE_SHIFT 22
#define ESTAT_IS_MASK 0x1fff	  // 12:0
#define ESTAT_ECODE_MASK 0x3f	  // 21:16
#define ESTAT_ESUBCODE_MASK 0x1ff // 30:22

/* LoongArch primary exception codes (ESTAT.Ecode). */
#define LA_ECODE_PIL 0x1  /* Invalid page on load. */
#define LA_ECODE_PIS 0x2  /* Invalid page on store. */
#define LA_ECODE_PIF 0x3  /* Invalid page on instruction fetch. */
#define LA_ECODE_PME 0x4  /* Page modification exception. */
#define LA_ECODE_PNR 0x5  /* Page is not readable. */
#define LA_ECODE_PNX 0x6  /* Page is not executable. */
#define LA_ECODE_PPI 0x7  /* Invalid page privilege. */
#define LA_ECODE_ADEF 0x8 /* Address error on instruction fetch. */
#define LA_ECODE_ALE 0x9  /* Address alignment error. */
#define LA_ECODE_BCE 0xa  /* Bounds check exception. */
#define LA_ECODE_SYS 0xb  /* System call. */
#define LA_ECODE_BRK 0xc  /* Breakpoint exception. */
#define LA_ECODE_INE 0xd  /* Undefined instruction. */
#define LA_ECODE_IPE 0xe  /* Instruction privilege error. */
#define LA_ECODE_FPD 0xf  /* Floating-point instructions are disabled. */

static inline int is_interrupt(uint64 estat)
{
	// In the unified VS=0 entry mode, an Ecode of zero denotes an interrupt.
	return ((estat >> ESTAT_ECODE_SHIFT) & ESTAT_ECODE_MASK) == 0;
}

static inline uint64 estat_ecode(uint64 estat)
{
	return (estat >> ESTAT_ECODE_SHIFT) & ESTAT_ECODE_MASK;
}

static inline uint64 estat_esubcode(uint64 estat)
{
	return (estat >> ESTAT_ESUBCODE_SHIFT) & ESTAT_ESUBCODE_MASK;
}

static inline uint64 estat_is(uint64 estat)
{
	return estat & ESTAT_IS_MASK;
}

static inline int estat_is_page_fault(uint64 estat)
{
	uint64 ecode = estat_ecode(estat);
	return ecode == LA_ECODE_PIL || ecode == LA_ECODE_PIS ||
	       ecode == LA_ECODE_PIF;
}

#endif
