#ifndef FV_LOONGARCH_PLATFORM_EIOINTC_H
#define FV_LOONGARCH_PLATFORM_EIOINTC_H

#include <kernel/types.h>

int eiointc_configure_vector(uint32 vector);
int eiointc_init(void);
void eiointc_enable_vector(uint32 vector);
void eiointc_disable_vector(uint32 vector);
void eiointc_ack_vector(uint32 vector);
uint32 eiointc_pending(uint32 group);

/* CPU HWI1 is bit 3 in ESTAT.IS and ECFG.LIE; this is not a vector ID. */
#define EIOINTC_CPU_IRQ (1ULL << 3)

#endif
