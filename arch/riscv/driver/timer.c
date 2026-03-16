#include "asm/riscv.h"
#include "asm/trap.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "platform/defs.h"

void pre_timerinit() {
  uint64 mie = r_mie();
  mie &= ~(MIE_MSIE | MIE_MEIE);
  mie |= MIE_MTIE;
  w_mie(mie);

  w_mcounteren(0xffff);
}

void timerinit() {
  LOG_INFO("Enable time interrupts...");
  w_sie(r_sie() | SIE_STIE);
  sbi_set_timer(r_time() + 1000000);
  w_sstatus(r_sstatus() | SSTATUS_SIE);
  LOG_INFO("Timer init done");
}
