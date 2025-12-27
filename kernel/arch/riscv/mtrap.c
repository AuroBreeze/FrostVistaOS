#include "driver/clint.h"
#include "driver/sbi.h"
#include "kernel/defs.h"
#include "kernel/riscv.h"
#include "kernel/trap.h"
#include "kernel/types.h"

void m_trap(uint64 mcause, uint64 mepc, uint64 *regs) {
  // Check whether the most significant bit is ahn exception or an interrupt
  int is_interrupt = (mcause >> 63) & 1;

  uint64 code = mcause & ((1ULL << 63) - 1);

  // BUG: Ban kprintf and panic
  // Because the current SP is not in a valid state, the SP has already been
  // saved, and the current SP is in an undefined state.
  if (is_interrupt) {
    // PERF: The code block where 'code == 7' cannot be removed. At the
    // beginning of the 'mstart' function, we set the interrupt enbale bit for
    // 'mie.MTIE', and this code block is the only one capable of handling the
    // MTIP branch. Removing it would cause the OS to hang immediately

    // NOTE: The entire CLINT trigger sequence is initiated via an SBI call,
    // which sets the next response time, clears the STIP suspend bit from the
    // previous cycle, and then waits for the specified duration. Upon time
    // expiration, CLINT generates an MTI interrupt, entering the M-mode
    // handling entry point. It proceeds to the code segment where code == 7,
    // sets the next time to infinity to prevent infinite loops, then sets the
    // STIP bit. It enters the S-mode interrupt handler, responds to cause == 5,
    // and subsequently re-enters the M-mode interrupt function. Here, it uses
    // SBI to set the next response time and clears STIP to avoid infinite
    // loops.

    if (code == 7) {
      uint64 *mtimecmp = (uint64 *)CLINT_MTIMECMP(r_mhartid());
      *mtimecmp = -1ULL; // Set the distance to infinity

      w_mip(r_mip() |
            MIP_STIP); // Key: Set STIP to allow S to receive scause = 5
    }
    return;
  } else {
    if (code == 9) {         // ecall from S
      uint64 eid = regs[16]; // a7
      uint64 fid = regs[15]; // a6
      uint64 arg0 = regs[9]; // a0 = stime_value

      if ((eid == SBI_EID_TIME && fid == SBI_FID_SET_TIMER) ||
          (eid == SBI_EID_LEGACY_SET_TIMER)) {

        uint64 *mtimecmp = (uint64 *)CLINT_MTIMECMP(r_mhartid());
        *mtimecmp = arg0;

        regs[9] = SBI_SUCCESS; // a0
        regs[10] = 0;          // a1
      } else {
        regs[9] = -2; // SBI_ERR_NOT_SUPPORTED
        regs[10] = 0;
      }

      w_mip(r_mip() & ~MIP_STIP);

      w_mepc(mepc + 4);
      return;
    } else {

      while (1)
        ;
    }
  }
}
