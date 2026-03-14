#include "driver/uart.h"
#include "kernel/defs.h"
#include "kernel/kalloc.h"
#include "kernel/log.h"
#include "kernel/mm.h"
#include "kernel/riscv.h"
#include "kernel/trap.h"

// define the kernelvec function in assembly
extern void kernelvec(void);
void trapinit() { w_stvec((uint64)kernelvec); }

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
  // kprintf("\nkalloc idle high addr counts: %d\tlow addr ptr: %p\n", FMM.size,
  //         ekalloc_ptr);
}

void s_trap_handler(void) {
  uint64 sc = r_scause();
  uint64 epc = r_sepc();
  uint64 tval = r_stval();

  if ((sc >> 63) == 1) {
    uint64 cause = sc & ((1ULL << 63) - 1);
    // determine if it's a timer interrupt
    // wo notify S state by setting SIP_SSIP (Software Interrupt Pending)
    if (cause == E_S_TIMER_INTERRUPT) {
      // timer interrupt
      // set timer for next interrupt
      sbi_set_timer(r_time() + 1000000);
      LOG_TRACE("Tick");
      return;
    }
    if (cause == E_S_EXTERNAL_INTERRUPT) {
      int id = cupid();
      int context = 2 * id + 1;

      int irq = plic_claim_interrupt(context);
      if (irq == UART_IRQ) {
        uartintr();
      } else if (irq != 0) {
        LOG_ERROR("SEI: unexpected irq=%d\n", irq);
      }
      if (irq) {
        plic_complete_interrupt(context, irq);
      }

      return;
    }
  }

  LOG_ERROR("=== TRAP ===");
  LOG_ERROR("Interrupt: %d, Exception: %d", (sc >> 63) == 1, (sc >> 63) == 0);
  LOG_ERROR("scause=%p sepc=%p stval=%p", (void *)sc, (void *)epc,
            (void *)tval);

  detail_kernel_trap_print(tval);

  // Simple Tips
  if ((sc >> 63) == 0) {
    switch (sc) {
    case I_S_ILLEGAL_INSTRUCTION:
      LOG_ERROR("cause: illegal instruction");
      break;
    case I_S_BREAKPOINT:
      LOG_ERROR("cause: breakpoint");
      epc = next_pc(epc);
      w_sepc(epc);
      return;
    case I_S_INSTRUCTION_PAGE_FAULT:
      LOG_ERROR("cause: instruction page fault");
      break;
    case I_S_LOAD_PAGE_FAULT:
      LOG_ERROR("cause: load page fault");
      break;
    case I_S_STORE_PAGE_FAULT:
      LOG_ERROR("cause: store/amo page fault");
      break;
    }
  }

  panic("panic trap");
}

void usertrap(void) {
  if ((r_sstatus() & SSTATUS_U_SPP) != 0) {
    panic("usertrap: not from user mode");
  }

  uint64 cause = r_scause();
  uint64 epc = r_sepc();

  LOG_INFO("\033[1;32m[VICTORY] User Trap Caught!\033[0m");
  LOG_INFO("scause: %d, sepc: 0x%p", cause, epc);

  if (cause == 8) {
    LOG_INFO("Target Eliminated: Successfully executed 'ecall' in U-mode!");

    w_sepc(epc + 4);
  } else {
    LOG_ERROR("Unexpected trap, cause: %d", cause);
    while (1)
      ;
  }
}
