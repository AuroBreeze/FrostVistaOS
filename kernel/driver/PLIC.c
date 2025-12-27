#include "driver/PLIC.h"
#include "driver/uart.h"
#include "kernel/defs.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

void plic_init_uart(void) {
  int id = cupid();
  int context = 2 * id + 1; // S-mode context

  plic_set_priority(UART_IRQ, 1);
  plic_enable_interrupt(context, UART_IRQ);
  plic_set_threshold(context, 0);
}

void plic_set_priority(int id, int priority) {
  *reg32(PLIC_IP_ADR(id)) = (uint32)priority;
}

int plic_enable_interrupt(int context, int id) {
  uint32 word = (uint32)id / 32;
  uint32 bit = (uint32)id % 32;
  volatile uint32 *enable = reg32(PLIC_IE_ADR(context, word));
  *enable |= (1U << bit);
  return 0;
}

int plic_set_threshold(int context, int threshold) {
  *reg32(PLIC_THRESHOLD_ADR(context)) = (uint32)threshold;
  return 0;
}

int plic_claim_interrupt(int context) {
  return (int)(*reg32(PLIC_CLAIM_ADR(context)));
}

int plic_complete_interrupt(int context, int id) {
  *reg32(PLIC_IC_ADR(context)) = (uint32)id;
  return 0;
}
