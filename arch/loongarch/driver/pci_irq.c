#define LOG_MODULE "IRQ"

#include <asm/irq.h>
#include <driver/pci.h>
#include <kernel/defs.h>
#include <kernel/irq.h>
#include <kernel/log.h>
#include <platform/eiointc.h>
#include <platform/pch_pic.h>

static uint8 registered[64] = {0};
static int ready = 0;

int platform_irq_init(void)
{
	if (ready)
		return 0;
	int saved = irq_save();
	if (eiointc_init() < 0 || pch_pic_init() < 0) {
		irq_restore(saved);
		return -1;
	}
	w_ecfg(r_ecfg() | EIOINTC_CPU_IRQ);
	ready = 1;
	irq_restore(saved);
	return 0;
}

/* Root-bus routing from this platform's PCI interrupt-map. */
static int pci_intx_irq(const struct pci_device *device)
{
	if (!device || device->bus != 0 || (device->header_type & 0x7f))
		return -1;
	uint8 pin = device->interrupt_pin;
	if (pin < 1 || pin > 4)
		return -1;
	return 16 + ((device->dev + pin - 1) & 3);
}

int arch_pci_intx_register(const struct pci_device *device,
			   irq_handler_t handler, void *arg)
{
	int irq = pci_intx_irq(device);
	if (!ready || irq < 0 || irq >= (int) pch_pic_irq_count() || !handler)
		return -1;
	int saved = irq_save();
	if (!registered[irq] &&
	    (pch_pic_configure(irq) < 0 || eiointc_configure_vector(irq) < 0)) {
		irq_restore(saved);
		return -1;
	}
	if (irq_register(irq, handler, arg) < 0) {
		irq_restore(saved);
		return -1;
	}
	registered[irq] = 1;
	LOG_INFO("PCI %d:%d.%d pin=%d IRQ=%d vector=%d registered", device->bus,
		 device->dev, device->func, device->interrupt_pin, irq, irq);
	irq_restore(saved);
	return 0;
}

int arch_pci_intx_enable(const struct pci_device *device)
{
	int irq = pci_intx_irq(device);
	if (!ready || irq < 0 || !registered[irq])
		return -1;
	int saved = irq_save();
	eiointc_enable_vector(irq);
	pch_pic_unmask(irq);
	irq_restore(saved);
	return 0;
}

void arch_pci_intx_disable(const struct pci_device *device)
{
	int irq = pci_intx_irq(device);
	if (!ready || irq < 0)
		return;
	int saved = irq_save();
	pch_pic_mask(irq);
	eiointc_disable_vector(irq);
	irq_restore(saved);
}

void platform_external_irq_dispatch(void)
{
	for (uint32 group = 0; group < IRQ_VECTOR_COUNT / 32; group++) {
		uint32 pending = eiointc_pending(group);
		while (pending) {
			uint32 bit = __builtin_ctz(pending);
			uint32 vector = (group * 32) + bit;
			pending &= pending - 1;
			/* Mask the level source, clear the CPU latch, then let
			 * the device handler deassert INTx before unmasking it
			 * again. */
			if (vector < pch_pic_irq_count())
				pch_pic_mask(vector);
			eiointc_ack_vector(vector);
			if (irq_dispatch(vector)) {
				if (vector < pch_pic_irq_count())
					pch_pic_unmask(vector);
			} else {
				eiointc_disable_vector(vector);
				LOG_WARN(
				    "Masked unhandled external IRQ vector %d",
				    vector);
			}
		}
	}
}
