#include "kernel/defs.h"
#include "kernel/mm.h"
#include "kernel/riscv.h"

extern pagetable_t kernel_table;
void s_trap_handler(void) {
  uint64 sc = r_scause();
  uint64 epc = r_sepc();
  uint64 tval = r_stval();

  kprintf("\n=== TRAP ===\n");
  kprintf("scause=%p sepc=%p stval=%p\n", (void *)sc, (void *)epc,
          (void *)tval);

  // WARNING: current the extern kernel page table is being used directly;
  // optimization is required later
  uint64 pa = walk_addr(kernel_table, tval);
  kprintf("pa: %p\n", pa);

  extern int cnt;
  kprintf("kalloc idle page counts: %d\n", cnt);

  // Simple Tips
  if ((sc >> 63) == 0) {
    switch (sc) {
    case 2:
      kprintf("cause: illegal instruction\n");
      break;
    case 3:
      kprintf("cause: breakpoint\n");
      epc = next_pc(epc);
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
