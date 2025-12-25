#include "driver/sbi.h"
#include "kernel/defs.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

// kernel/sbi.c
static inline long sbi_ecall(long eid, long fid, long a0, long a1, long a2,
                             long a3, long a4, long a5, long *out_val) {
  register long _a0 asm("a0") = a0;
  register long _a1 asm("a1") = a1;
  register long _a2 asm("a2") = a2;
  register long _a3 asm("a3") = a3;
  register long _a4 asm("a4") = a4;
  register long _a5 asm("a5") = a5;
  register long _a6 asm("a6") = fid;
  register long _a7 asm("a7") = eid;
  asm volatile("ecall"
               : "+r"(_a0), "+r"(_a1)
               : "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5), "r"(_a6), "r"(_a7)
               : "memory");
  if (out_val)
    *out_val = _a1;
  return _a0; // error
}

void sbi_set_timer(uint64 stime_value) {
  sbi_ecall(SBI_EID_TIME, SBI_FID_SET_TIMER, stime_value, 0, 0, 0, 0, 0, 0);
}
