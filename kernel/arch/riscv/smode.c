#include "kernel/smode.h"
#include "kernel/defs.h"
#include "kernel/kalloc.h"
#include "kernel/log.h"
#include "kernel/machine.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

void display_banner(void) {
  LOG_INFO("    ______                __ _    ___      __       ");
  LOG_INFO("   / ____/________  _____/ /| |  / (_)____/ /_____ _");
  LOG_INFO("  / /_  / ___/ __ \\/ ___/ __/ | / / / ___/ __/ __ `/");
  LOG_INFO(" / __/ / /  / /_/ (__  ) /_ | |/ / (__  ) /_/ /_/ / ");
  LOG_INFO("/_/   /_/   \\____/____/\\__/ |___/_/____/\\__/\\__,_/");
}

int early_mode = 1;

void user_mode_start();

void __attribute__((noreturn)) high_mode_start() {
  LOG_TRACE("Successfully jumped to high address!");
  LOG_TRACE("Current CUPID: %d", cupid());

  trapinit();
  uint64 current_sp;
  asm volatile("mv %0, sp" : "=r"(current_sp));
  LOG_TRACE("current_sp: %p", current_sp);
  kalloc_init(); // get memory

  clear_low_memory_mappings();
  LOG_INFO("Hello FrostVista OS!");

  user_mode_start();
  // main();
  while (1) {
  }
}

pagetable_t creat_user_pagetabel() {
  pagetable_t user_pagetable = (pagetable_t)kalloc();
  if (user_pagetable == 0) {
    panic("Failed to allocate memory");
  }

  memset(user_pagetable, 0, PGSIZE);

  for (int i = 256; i < 512; i++) {
    user_pagetable[i] = kernel_table[i];
  }

  return user_pagetable;
}

void usertrap(void) {
#define SSTATUS_SPP (1L << 8)
  if ((r_sstatus() & SSTATUS_SPP) != 0) {
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

void user_mode_start() {
  LOG_INFO("Switch to user mode...");

  uint64 user_code_table = (uint64)kalloc();
  if (user_code_table == 0) {
    panic("Failed to allocate memory");
  }
  uint64 user_stack = (uint64)kalloc();
  if (user_stack == 0) {
    panic("Failed to allocate memory");
  }
  // ecall
  // j .
  uint32 user_code[2] = {0x00000073, 0x0000006f};
  memcpy((uint64 *)user_code_table, user_code, 8);

  pagetable_t user_table = creat_user_pagetabel();
#include "kernel/machine.h"
#include "kernel/mm.h"

#define UART0_BASE 0x10000000UL

  kvmmap(user_table, 0x0, (uint64)ADR2LOW(user_code_table), PGSIZE,
         PTE_U | PTE_R | PTE_W | PTE_X | PTE_V);
  uint64 user_stack_va = 0x40000;
  kvmmap(user_table, (uint64)user_stack_va, (uint64)ADR2LOW(user_stack), PGSIZE,
         PTE_U | PTE_R | PTE_W | PTE_V);
  kvmmap(user_table, UART0_BASE, UART0_BASE, PGSIZE, PTE_R | PTE_W | PTE_V);
  if IS_ADR_HIGHT (user_table) {
    user_table = (pagetable_t)ADR2LOW((uint64)user_table);
  }

  // LOG_INFO("user_table: %p", user_table);
  uint64 x = r_sstatus();
  x &= ~(1L << 8); // SPP S --> U
  x |= (1L << 5);  // SPIE interrupt enable
  w_sstatus(x);

  w_sepc(0x0);

  uint64 user_stack_top = (uint64)user_stack_va + PGSIZE;

  sfence_vma();
  w_satp((8L << 60) | (uint64)(user_table) >> 12);
  sfence_vma();

  extern void uservec();
  uint64 trap_addr = (uint64)uservec;
  if IS_ADR_LOW (trap_addr) {
    trap_addr = ADR2HIGHT(trap_addr);
  }
  w_stvec(trap_addr);

  uint64 kernel_sp;
  asm volatile("mv %0, sp" : "=r"(kernel_sp));
  if IS_ADR_LOW (kernel_sp) {
    kernel_sp = ADR2HIGHT(kernel_sp);
  }
  asm volatile("csrw sscratch, %0\n\t"
               "mv sp, %1\n\t"
               "sret\n\t"
               :
               : "r"(kernel_sp), "r"(user_stack_top)
               : "memory");
  __builtin_unreachable();
}

void s_mode_start() {
  trapinit();

  plic_init_uart();
  pre_uart_init();

  uart_init();

  display_banner();
  // kprintf("FrostVistaOS booting...\n");
  LOG_INFO("FrostVistaOS booting...");

  timerinit();

  kvminit();
  kvminithart();

  early_mode = 0;

  uint64 target = (uint64)high_mode_start + KERNEL_VIRT_OFFSET;
  switch_to_high_address(target, KERNEL_VIRT_OFFSET);

  while (1) {
  }
}
