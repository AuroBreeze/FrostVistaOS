#include "driver/clint.h"
#include "kernel/defs.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

void pre_timerinit() {
  uint64 mie = r_mie();
  mie &= ~(MIE_MSIE | MIE_MEIE);
  mie |= MIE_MTIE;
  w_mie(mie);

  w_mcounteren(0xffff);
}

void timerinit() {
  kprintf("Enable time interrupts...\n");

  w_sie(r_sie() | SIE_STIE);
  sbi_set_timer(r_time() + 1000000);
  w_sstatus(r_sstatus() | SSTATUS_SIE);
  kprintf("Enabled\n");
}
