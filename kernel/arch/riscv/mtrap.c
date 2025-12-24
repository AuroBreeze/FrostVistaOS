#include "driver/clint.h"
#include "driver/sbi.h"
#include "kernel/defs.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

#define SBI_EID_TIME 0x54494D45ULL // 'TIME'
#define SBI_FID_SET_TIMER 0
#define SBI_EID_LEGACY_SET_TIMER 0 // legacy
void m_trap(uint64 mcause, uint64 mepc, uint64 *regs) {
  // Check whether the most significant bit is ahn exception or an interrupt
  int is_interrupt = (mcause >> 63) & 1;

  uint64 code = mcause & ((1ULL << 63) - 1);

  // WARNING: Ban kprintf and panic
  // Because the current SP is not in a valid state, the SP has already been
  // saved, and the current SP is in an undefined state.
  if (is_interrupt) {
    if (code == 7) {
      uint64 *mtimecmp = (uint64 *)CLINT_MTIMECMP(r_mhartid());
      *mtimecmp = -1ULL; // 先关掉下一次 MTI，等待 S 再 set_timer

      w_mip(r_mip() | MIP_STIP); // 关键：置 STIP，让 S 收到 scause=5
    }
  } else {
    if (code == 9) {         // ecall from S
      uint64 eid = regs[16]; // a7
      uint64 fid = regs[15]; // a6
      uint64 arg0 = regs[9]; // a0 = stime_value

      if ((eid == SBI_EID_TIME && fid == SBI_FID_SET_TIMER) ||
          (eid == SBI_EID_LEGACY_SET_TIMER)) {

        uint64 *mtimecmp = (uint64 *)CLINT_MTIMECMP(r_mhartid());
        *mtimecmp = arg0;

        // 标准返回：a0=error, a1=value
        regs[9] = SBI_SUCCESS; // a0
        regs[10] = 0;          // a1
      } else {
        // 不支持的 SBI 调用
        regs[9] = -2; // SBI_ERR_NOT_SUPPORTED（你也可以用自己定义的错误码）
        regs[10] = 0;
      }

      w_mip(r_mip() & ~MIP_STIP);

      w_mepc(mepc + 4);
      return;
    } else {

      while (1)
        ;
      uint64 mie = r_mie();
      if (code == 11)
        mie &= ~MIE_MEIE;
      if (code == 3)
        mie &= ~MIE_MSIE;
      w_mie(mie);
    }
  }
}
