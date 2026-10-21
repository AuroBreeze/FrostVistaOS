#ifndef FV_KERNEL_IRQ_H
#define FV_KERNEL_IRQ_H

#include "kernel/types.h"

#define IRQ_VECTOR_COUNT 256
#define IRQ_SHARED_HANDLERS 4

/* Return nonzero only if this device owned the interrupt. */
typedef int (*irq_handler_t)(void *arg);

int irq_register(uint32 vector, irq_handler_t handler, void *arg);
int irq_dispatch(uint32 vector);

struct pci_device;
/* A new platform source remains masked until enable; shared lines may already
 * be active, so the registering device must keep its own INTx masked. */
int arch_pci_intx_register(const struct pci_device *device,
			   irq_handler_t handler, void *arg);
int arch_pci_intx_enable(const struct pci_device *device);
void arch_pci_intx_disable(const struct pci_device *device);

#endif
