#include "driver/uart.h"
#include "kernel/kalloc.h"
#include "kernel/printf.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

extern void kernelvec(void);

void main(void) {

  char *adr = (char *)kalloc();
  kprintf("kalloc get the adr: %p\n", adr);

  void *p1 = kalloc();
  void *p2 = kalloc();
  kprintf("p1=%p p2=%p diff=%p\n", p1, p2, (void *)((uint64)p1 - (uint64)p2));
  kfree(p1);
  void *p3 = kalloc();
  kprintf("p3=%p (expect p1)\n", p3);

  kprintf("\nbefore ebreak\n");
  asm volatile("ebreak");
  kprintf("\nafter ebreak\n");

  while (1) {
  }
}
