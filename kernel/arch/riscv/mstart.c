#include "kernel/riscv.h"
#include "kernel/types.h"

void main(void);

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

  uint64 medeleg = (1ULL << 2) |  // illegal instruction
                   (1ULL << 3) |  // breakpoint (ebreak)
                   (1ULL << 8) |  // ecall from U-mode
                   (1ULL << 12) | // instruction page fault
                   (1ULL << 13) | // load page fault
                   (1ULL << 15);  // store/AMO page fault
  w_medeleg(medeleg);

  w_mideleg(~0ULL);

  // set the starting position of the MEPC
  w_mepc((uint64)main);

  asm volatile("mret");

  __builtin_unreachable();
}
