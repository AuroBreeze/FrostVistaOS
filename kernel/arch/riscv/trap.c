#include "kernel/defs.h"
#include "kernel/kalloc.h"
#include "kernel/mm.h"
#include "kernel/riscv.h"

void detail_kernel_trap_print(uint64 tval) {

  extern pagetable_t kernel_table;
  // WARNING: current the extern kernel page table is being used directly;
  // optimization is required later

  // uint64 pa = walk_addr(kernel_table, tval);
  // kprintf("pa: %p\n", pa);
  // kprintf("perm: w: %d, r: %d, v: %d", (PTE_W & pa & 0x3ff),
  //         (PTE_R & pa & 0x3ff), (PTE_V & pa & 0x3ff));

  extern int cnt;
  extern struct freeMemory FMM;
  extern char *ekalloc_ptr;
  kprintf("\nkalloc idle high addr counts: %d\tlow addr ptr: %p\n", FMM.size,
          ekalloc_ptr);
}

void s_trap_handler(void) {
  uint64 sc = r_scause();
  uint64 epc = r_sepc();
  uint64 tval = r_stval();
  if (sc & 0x8000000000000000L) {
    uint64 cause = sc & 0xff;

    // determine if it's a timer interrupt
    // wo notify S state by setting SIP_SSIP (Software Interrupt Pending)
    if (cause == 1) {
      kprintf("Tick\n");
      // clear the interrupt pending bit
      // prevent re-entering the trap handler
      w_sip(r_sip() & ~2);

      return;
    }
  }

  kprintf("\n=== TRAP ===\n");
  kprintf("scause=%p sepc=%p stval=%p\n", (void *)sc, (void *)epc,
          (void *)tval);

  detail_kernel_trap_print(tval);

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
