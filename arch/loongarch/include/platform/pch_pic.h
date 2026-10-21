#ifndef FV_LOONGARCH_PLATFORM_PCH_PIC_H
#define FV_LOONGARCH_PLATFORM_PCH_PIC_H

#include "kernel/types.h"

int pch_pic_init(void);
int pch_pic_configure(uint32 irq);
void pch_pic_mask(uint32 irq);
void pch_pic_unmask(uint32 irq);
uint32 pch_pic_irq_count(void);

int platform_irq_init(void);
void platform_external_irq_dispatch(void);

#endif
