#include "kernel/defs.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

// kernel/sbi.c
void sbi_set_timer(uint64 stime_value) {
  register uint64 a0 asm("a0") = stime_value;
  register uint64 a7 asm("a7") = 0; // SBI_SET_TIMER EID
  asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}

void timer_handle() { sbi_set_timer(r_time() + 1000000); }
