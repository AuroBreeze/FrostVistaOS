#include "driver/clint.h"
#include "kernel/machine.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

void s_mode_start(void);
void timerinit();


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

  uint64 x = r_mstatus();
  // Set all lower than the two bits of the MPP to 1, then perform the AND
  // opertaion, Only retain the position that should be 1
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;

  w_mstatus(x);

  // delegate all interrupts and exceptions to S-mode
  w_medeleg(0xffff);
  w_mideleg(0xffff);

  timerinit();
  // set the starting position of the MEPC
  w_mepc((uint64)s_mode_start);

  asm volatile("mret");

  __builtin_unreachable();
}

void timerinit() {
  // get this core's hartid
  int id = r_mhartid();

  int interval = 1000000; // 10ms
  // When mtime >= mtimecmp, a core interrupt occurs
  // Set the next timer interrupt time
  *(uint64 *)CLINT_MTIMECMP(id) = *(uint64 *)CLINT_MTIME + interval;

  // mscratch is used to save per-hart scratch area for machine mode
  w_mscratch((uint64)&mscratch0[32 * id]);

  // Set the M-state interrupt vector entry
  extern void timervec();
  w_mtvec((uint64)timervec);

  // Turn on the interrupt switch
  // MSTATUS_MIE: global interrupt enable
  // MIE_MTIE: timer interrupt enable
  w_mstatus(r_mstatus() | MSTATUS_MIE);
  w_mie(r_mie() | MIE_MTIE);
}
