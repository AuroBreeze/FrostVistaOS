#ifndef FV_LOONGARCH_H
#define FV_LOONGARCH_H

#include "kernel/types.h"

/*
 * Boot code executes from the DMW0 window (0x8000...).  The regular kernel
 * text is linked in the canonical high half (0xffffffc0...).  At -O0 GCC may
 * emit out-of-line copies of these tiny CSR accessors, and a normal B26 call
 * cannot reach that copy from the boot section.  Keep the accessors embedded
 * at their call sites so debug builds retain the same boot layout as release
 * builds.
 */
#define LA_ALWAYS_INLINE static inline __attribute__((always_inline))

#define PRMD_PPLV_MASK 0x3ULL
#define PRMD_PIE (1ULL << 2)
#define PRMD_PWE (1ULL << 3)
#define PRMD_PPLV_PLV0 0
#define PRMD_PPLV_PLV3 3

// CRMD (0x0) controls the current privilege level, global interrupts, and
// address translation mode.
LA_ALWAYS_INLINE uint64 r_crmd()
{
	uint64 x;
	asm volatile("csrrd %0, 0x0" : "=r"(x));
	return x;
}

LA_ALWAYS_INLINE void w_crmd(uint64 x)
{
	asm volatile("csrwr %0, 0x0" : "+r"(x));
}

static inline uint64 r_sp()
{
	uint64 x;
	asm volatile("move %0, $sp" : "=r"(x));
	return x;
}

// PRMD (0x1) preserves the privilege level, interrupt state, and watchpoint
// state on entry to a regular exception.
static inline uint64 r_prmd()
{
	uint64 x;
	asm volatile("csrrd %0, 0x1" : "=r"(x));
	return x;
}

static inline void w_prmd(uint64 x)
{
	asm volatile("csrwr %0, 0x1" : "+r"(x));
}

// KScratch0-KScratch3 (0x30-0x33) are kernel scratch registers used by the
// exception entry path.
static inline uint64 r_kscratch0()
{
	uint64 x;
	asm volatile("csrrd %0, 0x30" : "=r"(x));
	return x;
}

static inline void w_kscratch0(uint64 x)
{
	asm volatile("csrwr %0, 0x30" : "+r"(x));
}

static inline uint64 r_kscratch1()
{
	uint64 x;
	asm volatile("csrrd %0, 0x31" : "=r"(x));
	return x;
}

static inline void w_kscratch1(uint64 x)
{
	asm volatile("csrwr %0, 0x31" : "+r"(x));
}

static inline uint64 r_kscratch2()
{
	uint64 x;
	asm volatile("csrrd %0, 0x32" : "=r"(x));
	return x;
}

static inline void w_kscratch2(uint64 x)
{
	asm volatile("csrwr %0, 0x32" : "+r"(x));
}

static inline uint64 r_kscratch3()
{
	uint64 x;
	asm volatile("csrrd %0, 0x33" : "=r"(x));
	return x;
}

static inline void w_kscratch3(uint64 x)
{
	asm volatile("csrwr %0, 0x33" : "+r"(x));
}

// DMW1 (0x181) defines the uncached direct-mapping window used for MMIO
// during early boot.
static inline uint64 r_dmw1()
{
	uint64 x;
	asm volatile("csrrd %0, 0x181" : "=r"(x));
	return x;
}

LA_ALWAYS_INLINE void w_dmw1(uint64 x)
{
	asm volatile("csrwr %0, 0x181" : "+r"(x));
}

// TLBIDX (0x10) contains the TLB index, page size, and entry validity state.
static inline uint64 r_tlbidx()
{
	uint64 x;
	asm volatile("csrrd %0, 0x10" : "=r"(x));
	return x;
}

static inline void w_tlbidx(uint64 x)
{
	asm volatile("csrwr %0, 0x10" : "+r"(x));
}

// TLBEHI (0x11) contains the virtual page number and ASID for a TLB entry.
static inline uint64 r_tlbehi()
{
	uint64 x;
	asm volatile("csrrd %0, 0x11" : "=r"(x));
	return x;
}

static inline void w_tlbehi(uint64 x)
{
	asm volatile("csrwr %0, 0x11" : "+r"(x));
}

// TLBELO0 (0x12) contains the low-order fields for the even-page TLB entry.
static inline uint64 r_tlbelo0()
{
	uint64 x;
	asm volatile("csrrd %0, 0x12" : "=r"(x));
	return x;
}

static inline void w_tlbelo0(uint64 x)
{
	asm volatile("csrwr %0, 0x12" : "+r"(x));
}

// TLBELO1 (0x13) contains the low-order fields for the odd-page TLB entry.
static inline uint64 r_tlbelo1()
{
	uint64 x;
	asm volatile("csrrd %0, 0x13" : "=r"(x));
	return x;
}

static inline void w_tlbelo1(uint64 x)
{
	asm volatile("csrwr %0, 0x13" : "+r"(x));
}

// TLBRENTRY (0x88) contains the physical address of the TLB refill handler.
static inline uint64 r_tlbrentry()
{
	uint64 x;
	asm volatile("csrrd %0, 0x88" : "=r"(x));
	return x;
}

LA_ALWAYS_INLINE void w_tlbrentry(uint64 x)
{
	asm volatile("csrwr %0, 0x88" : "+r"(x));
}

// TLBRBADV (0x89) contains the faulting virtual address for a TLB refill.
LA_ALWAYS_INLINE uint64 r_tlbrbadv()
{
	uint64 x;
	asm volatile("csrrd %0, 0x89" : "=r"(x));
	return x;
}

static inline void w_tlbrbadv(uint64 x)
{
	asm volatile("csrwr %0, 0x89" : "+r"(x));
}

// TLBRERA (0x8a) contains the TLB refill return address and exception-context
// flag.
static inline uint64 r_tlbrera()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8a" : "=r"(x));
	return x;
}

static inline void w_tlbrera(uint64 x)
{
	asm volatile("csrwr %0, 0x8a" : "+r"(x));
}

// TLBRSAVE (0x8b) is a software scratch register used by the TLB refill
// handler.
static inline uint64 r_tlbrsave()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8b" : "=r"(x));
	return x;
}

static inline void w_tlbrsave(uint64 x)
{
	asm volatile("csrwr %0, 0x8b" : "+r"(x));
}

// TLBRELO0 (0x8c) contains the low-order even-page fields for a TLB refill.
static inline uint64 r_tlbrelo0()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8c" : "=r"(x));
	return x;
}

LA_ALWAYS_INLINE void w_tlbrelo0(uint64 x)
{
	asm volatile("csrwr %0, 0x8c" : "+r"(x));
}

// TLBRELO1 (0x8d) contains the low-order odd-page fields for a TLB refill.
static inline uint64 r_tlbrelo1()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8d" : "=r"(x));
	return x;
}

LA_ALWAYS_INLINE void w_tlbrelo1(uint64 x)
{
	asm volatile("csrwr %0, 0x8d" : "+r"(x));
}

// TLBREHI (0x8e) contains the high-order fields for a TLB refill.
static inline uint64 r_tlbrehi()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8e" : "=r"(x));
	return x;
}

LA_ALWAYS_INLINE void w_tlbrehi(uint64 x)
{
	asm volatile("csrwr %0, 0x8e" : "+r"(x));
}

// TLBRPRMD (0x8f) preserves processor mode information for a TLB refill.
LA_ALWAYS_INLINE uint64 r_tlbrprmd()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8f" : "=r"(x));
	return x;
}

static inline void w_tlbrprmd(uint64 x)
{
	asm volatile("csrwr %0, 0x8f" : "+r"(x));
}

// Invalidate every TLB entry, equivalent to RISC-V sfence.vma zero, zero.
// LoongArch INVTLB operation 0 does not filter by address space or address.
LA_ALWAYS_INLINE void sfence_vma()
{
	asm volatile("invtlb 0x0, $zero, $zero" ::: "memory");
}

// Retain this name for compatibility with existing LoongArch boot code.
LA_ALWAYS_INLINE void invtlb_all()
{
	sfence_vma();
}

// PGDL (0x19) contains the page global directory base for the lower address
// space.
LA_ALWAYS_INLINE uint64 r_pgdl()
{
	uint64 x;
	asm volatile("csrrd %0, 0x19" : "=r"(x));
	return x;
}

LA_ALWAYS_INLINE void w_pgdl(uint64 x)
{
	asm volatile("csrwr %0, 0x19" : "+r"(x));
}

// PGDH (0x1a) contains the page global directory base for the upper address
// space.
LA_ALWAYS_INLINE uint64 r_pgdh()
{
	uint64 x;
	asm volatile("csrrd %0, 0x1a" : "=r"(x));
	return x;
}

LA_ALWAYS_INLINE void w_pgdh(uint64 x)
{
	asm volatile("csrwr %0, 0x1a" : "+r"(x));
}

// PGD (0x1b) is a read-only page global directory base selected according to
// the current BADV or TLBRBADV context.
static inline uint64 r_pgd()
{
	uint64 x;
	asm volatile("csrrd %0, 0x1b" : "=r"(x));
	return x;
}

// PWCL (0x1c) controls page-table walking for the lower address space.
static inline uint64 r_pwcl()
{
	uint64 x;
	asm volatile("csrrd %0, 0x1c" : "=r"(x));
	return x;
}

LA_ALWAYS_INLINE void w_pwcl(uint64 x)
{
	asm volatile("csrwr %0, 0x1c" : "+r"(x));
}

// PWCH (0x1d) controls page-table walking for the upper address space.
static inline uint64 r_pwch()
{
	uint64 x;
	asm volatile("csrrd %0, 0x1d" : "=r"(x));
	return x;
}

LA_ALWAYS_INLINE void w_pwch(uint64 x)
{
	asm volatile("csrwr %0, 0x1d" : "+r"(x));
}

// STLBPS (0x1e) configures the shared page size for the STLB; its PS field is
// the base-2 logarithm of the page size.
static inline uint64 r_stlbps()
{
	uint64 x;
	asm volatile("csrrd %0, 0x1e" : "=r"(x));
	return x;
}

LA_ALWAYS_INLINE void w_stlbps(uint64 x)
{
	asm volatile("csrwr %0, 0x1e" : "+r"(x));
}

// EENTRY (0xc) contains the base address for regular exception and interrupt
// handlers.
static inline void w_eentry(uint64 x)
{
	asm volatile("csrwr %0, 0xc" : "+r"(x));
}

// ECFG (0x4) controls exception-vector spacing and enables the 13 local
// interrupt sources.
static inline uint64 r_ecfg()
{
	uint64 x;
	asm volatile("csrrd %0, 0x4" : "=r"(x));
	return x;
}

static inline void w_ecfg(uint64 x)
{
	asm volatile("csrwr %0, 0x4" : "+r"(x));
}

// ESTAT (0x5) contains the exception code, exception subcode, and pending
// interrupt state.
static inline uint64 r_estat()
{
	uint64 x;
	asm volatile("csrrd %0, 0x5" : "=r"(x));
	return x;
}

// ERA (0x6) contains the return address saved on exception entry; ERTN resumes
// execution from this address.
static inline uint64 r_era()
{
	uint64 x;
	asm volatile("csrrd %0, 0x6" : "=r"(x));
	return x;
}

static inline void w_era(uint64 era)
{
	asm volatile("csrwr %0, 0x6" : "+r"(era));
}

// BADV (0x7) contains the faulting virtual address for address-related
// exceptions.
static inline uint64 r_badv()
{
	uint64 x;
	asm volatile("csrrd %0, 0x7" : "=r"(x));
	return x;
}

// BADI (0x8) contains the encoding of the instruction that caused an
// exception.
static inline uint64 r_badi()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8" : "=r"(x));
	return x;
}

// CPUID (0x20) contains the logical identifier of the current processor core.
static inline uint64 r_cpuid()
{
	uint64 x;
	asm volatile("csrrd %0, 0x20" : "=r"(x));
	return x;
}

// TID (0x40) contains the programmable identifier of the current core's
// timer.
static inline uint64 r_tid()
{
	uint64 x;
	asm volatile("csrrd %0, 0x40" : "=r"(x));
	return x;
}

static inline void w_tid(uint64 x)
{
	asm volatile("csrwr %0, 0x40" : "+r"(x));
}

// TCFG (0x41) controls the timer's initial value, periodic mode, and enable
// state.
static inline uint64 r_tcfg()
{
	uint64 x;
	asm volatile("csrrd %0, 0x41" : "=r"(x));
	return x;
}

static inline void w_tcfg(uint64 x)
{
	asm volatile("csrwr %0, 0x41" : "+r"(x));
}

// TVAL (0x42) contains the timer's current countdown value and is read-only.
static inline uint64 r_tval()
{
	uint64 x;
	asm volatile("csrrd %0, 0x42" : "=r"(x));
	return x;
}

// CNTC (0x43) contains the signed compensation applied to the constant-
// frequency counter.
static inline uint64 r_cntc()
{
	uint64 x;
	asm volatile("csrrd %0, 0x43" : "=r"(x));
	return x;
}

static inline void w_cntc(uint64 x)
{
	asm volatile("csrwr %0, 0x43" : "+r"(x));
}

// Writing one to TICLR (0x44) bit 0 clears the timer interrupt; reads always
// return zero.
static inline uint64 r_ticlr()
{
	uint64 x;
	asm volatile("csrrd %0, 0x44" : "=r"(x));
	return x;
}

static inline void w_ticlr(uint64 x)
{
	asm volatile("csrwr %0, 0x44" : "+r"(x));
}

#endif
