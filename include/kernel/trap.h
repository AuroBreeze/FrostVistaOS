#ifndef TRAP_H
#define TRAP_H

#define MIE_MTIE (1UL << 7) // MTIE : Machine Timer Interrupt Enable
#define MIE_MSIE (1UL << 3)
#define MIE_MEIE (1UL << 7)

#define MIP_STIP (1UL << 5) // STIP : Supervisor Timer Interrupt Pending
#define MIP_SSIP (1UL << 1)

#define MSTATUS_MIE (1UL << 3)
#define MSTATUS_MPIE (1UL << 7)
#define MSTATUS_SPIE (1UL << 5)

#define SSTATUS_SIE (1UL << 1)
#define SIE_SSIE (1UL << 1)
#define SIE_STIE (1UL << 5)
#define SIE_SEIE (1UL << 9)

#endif
