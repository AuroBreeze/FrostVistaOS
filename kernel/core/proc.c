#include "kernel/riscv.h"
#include "kernel/types.h"

int cupid() {
  int id = r_tp();
  return id;
}
