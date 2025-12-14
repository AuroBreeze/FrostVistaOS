#include "kernel/printf.h"
#include "kernel/riscv.h"

void s_trap_handler(void) {
  uint64 sc = r_scause();
  uint64 epc = r_sepc();
  uint64 tval = r_stval();

  kprintf("\n=== TRAP ===\n");
  kprintf("scause=%p sepc=%p stval=%p\n", (void *)sc, (void *)epc,
          (void *)tval);

  // Simple Tips
  if ((sc >> 63) == 0) {
    switch (sc) {
    case 2:
      kprintf("cause: illegal instruction\n");
      break;
    case 3:
      kprintf("cause: breakpoint\n");

      // 取 sepc 处的低 16 位判断是否压缩指令
      uint16 insn16 = *(uint16 *)(epc);
      if ((insn16 & 0x3) != 0x3)
        epc += 2; // 16-bit compressed
      else
        epc += 4; // normal 32-bit

      w_sepc(epc);
      return;
    case 12:
      kprintf("cause: instruction page fault\n");
      break;
    case 13:
      kprintf("cause: load page fault\n");
      break;
    case 15:
      kprintf("cause: store/amo page fault\n");
      break;
    }
  }

  panic("panic trap\n");
}
