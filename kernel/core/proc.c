#include "asm/cpu.h"
#include "kernel/defs.h"
#include "kernel/types.h"

int cpuid() {
  int id = hal_get_cpu_id();
  return id;
}
