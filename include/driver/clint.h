#ifndef CLINT_H
#define CLINT_H

#define CLINT_BASE 0x02000000UL

#define MIE_MTIE (1UL << 7) // MTIE : Machine Timer Interrupt Enable
#define MSTATUS_MIE (1UL << 3)
#define MSTATUS_MPIE (1UL << 7)
#define MSTATUS_SPIE (1UL << 5)

#define MIP_STIP (1UL << 5) // STIP : Supervisor Timer Interrupt Pending

#define SSTATUS_SIE (1UL << 1)
#define SIE_SSIE (1UL << 1)
#define SIE_STIE (1UL << 5)
// Each core(Hart) has one. with an offset of 0x4000
#define CLINT_MTIMECMP(hartid) (CLINT_BASE + 0x4000 + 8 * (hartid))

// mtime register: current time
// All core share one, with an offset of 0xBFF8
#define CLINT_MTIME (CLINT_BASE + 0xBFF8)

#endif
