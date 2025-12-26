#ifndef PLIC_H
#define PLIC_H

#include "kernel/types.h"

#define PLIC_BASE 0x0C000000
#define PLIC_MM_SIZE 0x400000
#define BASE_ADR(OFFSET) (PLIC_BASE + OFFSET)

#define PLIC_PRIORITY_OFFSET 0x000000
#define PLIC_PENDING_OFFSET 0x001000
#define PLIC_IE_OFFSET 0x002000
#define PLIC_THRESHOLD_OFFSET 0x200000
#define PLIC_CLAIM_OFFSET 0x200004
#define PLIC_IC_OFFSET 0x200004

// NOTE:
// id: interrupt source ID
// 1 <= id <= 1023

#define PLIC_IP_ADR(id) (BASE_ADR(PLIC_PRIORITY_OFFSET) + 4 * (id))
#define PLIC_IPB_ADR(id) (BASE_ADR(PLIC_PENDING_OFFSET) + 4 * ((id) / 32))
#define PLIC_IE_ADR(context, word)                                             \
  (BASE_ADR(PLIC_IE_OFFSET) + 0x80 * (context) + 4 * word)
#define PLIC_THRESHOLD_ADR(context)                                            \
  (BASE_ADR(PLIC_THRESHOLD_OFFSET) + 0x1000 * (context))
#define PLIC_CLAIM_ADR(context)                                                \
  (BASE_ADR(PLIC_CLAIM_OFFSET) + 0x1000 * (context))
#define PLIC_IC_ADR(context) (BASE_ADR(PLIC_IC_OFFSET) + 0x1000 * (context))

static inline volatile uint32 *reg32(uint64 addr) {
  return (volatile uint32 *)addr;
}
#endif
