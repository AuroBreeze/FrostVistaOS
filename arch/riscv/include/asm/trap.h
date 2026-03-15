#ifndef TRAP_H
#define TRAP_H

#include "asm/riscv.h"

#define MIE_MTIE (1UL << 7) // MTIE : Machine Timer Interrupt Enable
#define MIE_MSIE (1UL << 3)
#define MIE_MEIE (1UL << 7)

#define MIP_STIP (1UL << 5) // STIP : Supervisor Timer Interrupt Pending
#define MIP_SSIP (1UL << 1)

#define MSTATUS_MIE (1UL << 3)
#define MSTATUS_MPIE (1UL << 7)
#define MSTATUS_SPIE (1UL << 5)

#define SSTATUS_SIE (1UL << 1) // SIE : Supervisor Interrupt Enable
#define SSTATUS_SPIE (1UL << 5) // SPIE : Supervisor Privileged Interrupt Enable
#define SIE_SSIE (1UL << 1)
#define SIE_STIE (1UL << 5)
#define SIE_SEIE (1UL << 9)

#define SSTATUS_U_SPP (1UL << 8) // Set User-Mode Supervisor Privilege Level

#define E_S_TIMER_INTERRUPT (5) // Exception-level Supervisor Timer Interrupt
#define E_S_EXTERNAL_INTERRUPT (9) // Exception-level Supervisor External Interrupt

#define I_S_INSTRUCTION_ADDRESS_MISALIGNED (0) // Instruction Address Misaligned
#define I_S_INSTRUCTION_ACCESS_FAULT (1)         // Instruction Access Fault
#define I_S_ILLEGAL_INSTRUCTION (2)              // Illegal Instruction
#define I_S_BREAKPOINT (3)                       // Breakpoint
#define I_S_LOAD_ADDRESS_MISALIGNED (4)          // Load Address Misaligned
#define I_S_LOAD_ACCESS_FAULT (5)                // Load Access Fault
#define I_S_STORE_ADDRESS_MISALIGNED (6)         // Store Address Misaligned
#define I_S_STORE_ACCESS_FAULT (7)               // Store Access Fault
#define I_S_ECALL_FROM_USER_MODE (8)             // eCall from User Mode
#define I_S_EBREAK (11)                          // eBreak
#define I_S_INSTRUCTION_PAGE_FAULT (12)          // Instruction Page Fault
#define I_S_LOAD_PAGE_FAULT (13)                 // Load Page Fault
#define I_S_STORE_PAGE_FAULT (15)                // Store Page Fault

// disable device interrupts
static inline void
intr_off()
{
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// enable device interrupts
static inline void
intr_on()
{
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

#endif
