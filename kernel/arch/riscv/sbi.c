#include "kernel/defs.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

void sbi_set_timer(uint64 stime_value) {
  asm volatile("mv a7, %0\n"
               "mv a0, %1\n"
               "ecall\n"
               :
               : "r"(0), "r"(stime_value)
               : "a0", "a7");
}

void timer_handle() { sbi_set_timer(r_time() + 1000000); }
