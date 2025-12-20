#include "kernel/tool.h"
#include "kernel/defs.h"
#include "kernel/types.h"

uint64 next_pc(uint64 epc) {
// Check the lower 16 bits at the sepc location to determine if it is a
// compressed instruction.
//
#ifdef DEBUG
  kprintf("\n before epc: %x\n", epc);
#endif

  uint64 temp = epc;
  uint16 insn16 = *(uint16 *)(epc);
  if ((insn16 & 0x3) != 0x3)
    epc += 2; // 16-bit compressed
  else
    epc += 4; // normal 32-bit

#ifdef DEBUG
  kprintf("\n after epc: %x\n", epc);
  kprintf("\n %d\n", epc - temp);
#endif

  return epc;
}

// Acquire memory based on the current pattern
uint64 pt_alloc_page_pa() {
  if (early_mode) {
    return (uint64)ekalloc();
  } else {
    return (uint64)kalloc();
  }
}
