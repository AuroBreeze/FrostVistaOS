#include "driver/clint.h"
#include "driver/sbi.h"
#include "kernel/defs.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

void m_trap(uint64 mcause, uint64 mepc, uint64 *regs) {
  // Check whether the most significant bit is ahn exception or an interrupt
  int is_interrupt = (mcause >> 63) & 1;
  int code = mcause & 0xff;
  // WARNING: Ban kprintf and panic
  // Because the current SP is not in a valid state, the SP has already been
  // saved, and the current SP is in an undefined state.
  if (is_interrupt) {
    if (code == 7) {
      int hart_id = r_mhartid();
      uint64 *mtimecmp = (uint64 *)CLINT_MTIMECMP(hart_id);
      *mtimecmp = -1ULL;

      w_mip(r_mip() | MIP_SSIP);
    }
  } else {
    if (code == 9) {
      // According to the SBI manual, the EID is placed in a7 register
      uint64 eid = regs[17];
      uint64 arg0 = regs[10]; // a0 register stores the value

      if (eid == SBI_SET_TIMER) {
        uint64 *mtimecmp = (uint64 *)CLINT_MTIMECMP(r_mhartid());
        *mtimecmp = arg0;

        // Clear the flag indicating the last interrupt
        w_mip(r_mip() & ~MIP_SSIP);

        // set the return value
        regs[10] = SBI_SUCCESS;
      }

      // ecall is a 4-byte instruction
      // If 4 is not added, mret will return and then execute ecall again,
      // causing an infinite loop
      w_mepc(mepc + 4);
    } else {
      while (1)
        ;
    }
  }
}
