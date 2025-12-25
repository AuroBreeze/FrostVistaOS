#include "kernel/defs.h"
#include "kernel/machine.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

extern void s_mode_start(void);

extern void m_trap_handler(void);

// Register 32 is one of the 32 general-purpose registers in RISC-V architecture
// meaning it is one of the registers we need to save during a savepoint.
uint64 mscratch0[NCPU * 32];

__attribute__((noreturn)) void mstart(void) {
  // configuer PMP. PMP will perform checks when MPP is set to S or U in
  // mstatus. In PMP, the three permission bits R/X/W must be set
  //
  // here we intend to configuer addr0 and cfg0, setting their usable range with
  // an upper bound of pmpaddr0 and a lower bound of 0. Below is the original
  // text from the manual: If PMP entry 0’s A field is set to TOR, zero is used
  // for the lower bound, and so it matches any address y<pmpaddr0 end
  w_pmpaddr0(~0ULL);
  w_pmpcfg0(0x0f); // 0x0f = 0000_1111b: A-X-W-R

  // Disable interrupts
  w_satp(0);
  // Force refresh
  sfence_vma();

  int hartid = r_mhartid();
  w_mscratch((uint64)&mscratch0[hartid * 32]);
  w_mtvec((uint64)m_trap_handler);

  uint64 x = r_mstatus();
  // Set all lower than the two bits of the MPP to 1, then perform the AND
  // opertaion, Only retain the position that should be 1
  x &= ~MSTATUS_MPP_MASK;
  // Set S mode, but setting it does not switch to S mode immediately
  x |= MSTATUS_MPP_S;

  w_mstatus(x);

  pre_timerinit();

  // NOTE: Interrupt = 1
  // delegate all interrupts
  w_mideleg(0xffff);
  // NOTE: Interrupt = 0
  // delegate the following exceptions
  w_medeleg((1 << 1) | (1 << 3) | (1 << 8) | (1 << 13) | (1 << 15));

  // set the starting position of the MEPC
  w_mepc((uint64)s_mode_start);

  // will switch to S-mode and jump to the address in MEPC
  asm volatile("mret");

  __builtin_unreachable();
}
