#include "kernel/riscv.h"
#include "kernel/types.h"
#include "kernel/log.h"
#include "kernel/defs.h"
#include "kernel/kalloc.h"
#include "kernel/machine.h"
#include "kernel/mm.h"

#define UART0_BASE 0x10000000UL
extern pagetable_t kernel_table;

int cupid() {
  int id = r_tp();
  return id;
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

  uint64 kernel_sp = (uint64)kalloc();
  if (kernel_sp == 0) {
    panic("Failed to allocate memory");
  } else {
    // pointer to the top of the kernel stack
    //  Ensure at least 4KB of space to save registers, rather than relying on
    //  the kernel having free space
    kernel_sp = (uint64)kernel_sp + 0x1000;
  }
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
