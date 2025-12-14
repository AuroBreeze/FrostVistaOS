#include "driver/uart.h"
#include "kernel/kalloc.h"
#include "kernel/printf.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

void display_banner(void) {
  uart_puts("    ______                __ _    ___      __       \n");
  uart_puts("   / ____/________  _____/ /| |  / (_)____/ /_____ _\n");
  uart_puts("  / /_  / ___/ __ \\/ ___/ __/ | / / / ___/ __/ __ `/\n");
  uart_puts(" / __/ / /  / /_/ (__  ) /_ | |/ / (__  ) /_/ /_/ / \n");
  uart_puts("/_/   /_/   \\____/____/\\__/ |___/_/____/\\__/\\__,_/  \n");
}

extern void kernelvec(void);

void main(void) {
  w_stvec((uint64)kernelvec);
  uart_init();
  display_banner();
  kprintf("FrostVistaOS booting...\n");
  kprintf("Hello FrostVista OS!\n");
  kprintf("uart_init ok, x=%d, ptr=%p\n", 42, main);
  kalloc_init();

  char *adr = (char *)kalloc();
  kprintf("kalloc get the adr: %p\n", adr);

  void *p1 = kalloc();
  void *p2 = kalloc();
  kprintf("p1=%p p2=%p diff=%p\n", p1, p2, (void *)((uint64)p1 - (uint64)p2));
  kfree(p1);
  void *p3 = kalloc();
  kprintf("p3=%p (expect p1)\n", p3);

  uint64 y = r_sstatus();
  kprintf("sstatus: %x\n", y);

  uint64 x = r_mstatus();
  kprintf("mstatus: %x\n", x);
  kprintf("test ok\n");
  while (1) {
  }
}
