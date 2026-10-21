#define LOG_MODULE "PCH_PIC"

#include "kernel/arch/mm.h"
#include "kernel/log.h"
#include "platform/pch_pic.h"

/* PCH-PIC physical MMIO base on the QEMU LoongArch virt platform. */
#define PCH_PIC_PA 0x10000000ULL
/* Kernel VA mapped with uncached MMIO attributes by pch_pic_init(). */
#define PCH_PIC_VA (KERNEL_IO_BASE + PCH_PIC_PA)

/* Register offsets relative to PCH_PIC_PA/PCH_PIC_VA. */
/* 64-bit input mask bitmap: 1 = masked, 0 = unmasked. */
#define PIC_MASK 0x20U
/* HT message forwarding bitmap; 1 enables forwarding for an input.
 * This does not enable PCI MSI/MSI-X on the device. */
#define PIC_HTMSI 0x40U
/* Trigger bitmap: 1 = edge-triggered, 0 = level-triggered. */
#define PIC_EDGE 0x60U
/* One route byte per input; current platform routing uses 0x01. */
#define PIC_ROUTE 0x100U
/* One byte per input selecting its upstream EIOINTC vector. */
#define PIC_VECTOR 0x200U
/* Polarity bitmap: 0 = active high, 1 = active low. */
#define PIC_POLARITY 0x3e0U

static uint32 irq_count;

static uint32 pic_read32(uint32 offset)
{
	return *(volatile uint32 *) (PCH_PIC_VA + offset);
}

static void pic_write32(uint32 offset, uint32 value)
{
	*(volatile uint32 *) (PCH_PIC_VA + offset) = value;
}

static void pic_change_bit(uint32 reg, uint32 irq, int set)
{
	uint32 addr = reg + ((irq / 32) * 4);
	uint32 value = pic_read32(addr);
	uint32 bit = 1U << (irq % 32);
	pic_write32(addr, set ? value | bit : value & ~bit);
}

int pch_pic_init(void)
{
	if (irq_count)
		return 0;
	if (kvmmap_mmio_current(PCH_PIC_VA, PCH_PIC_PA, PGSIZE,
				LA_PTE_PLV0 | LA_PTE_W | LA_PTE_NX |
				    LA_PTE_MAT_SUC) < 0)
		return -1;
	arch_tlb_flush_all();

	// The number of supported interrupt sources. The actual number of
	// interrupts is equal to the value of this field plus 1.
	irq_count = ((pic_read32(4) >> 16) & 0xffU) + 1;
	if (!irq_count || irq_count > 64) {
		LOG_ERROR("Invalid PCH-PIC input count %d", irq_count);
		irq_count = 0;
		return -1;
	}

	// No input may arrive before its handler and route are installed.
	pic_write32(PIC_MASK, 0xffffffffU);
	pic_write32(PIC_MASK + 4, 0xffffffffU);
	LOG_INFO("PCH-PIC ready: %d inputs, all masked", irq_count);
	return 0;
}

uint32 pch_pic_irq_count(void)
{
	return irq_count;
}

void pch_pic_mask(uint32 irq)
{
	if (irq < irq_count)
		pic_change_bit(PIC_MASK, irq, 1);
}

void pch_pic_unmask(uint32 irq)
{
	if (irq < irq_count)
		pic_change_bit(PIC_MASK, irq, 0);
}

int pch_pic_configure(uint32 irq)
{
	if (irq >= irq_count)
		return -1;
	pch_pic_mask(irq);
	/* Current virt.dts specifies active-high, level-triggered PCI INTx. */
	pic_change_bit(PIC_EDGE, irq, 0);
	pic_change_bit(PIC_POLARITY, irq, 0);
	pic_change_bit(PIC_HTMSI, irq, 1);
	*(volatile uint8 *) (PCH_PIC_VA + PIC_ROUTE + irq) = 1;
	/* PCH-PIC input N feeds EIOINTC vector N on this platform. */
	*(volatile uint8 *) (PCH_PIC_VA + PIC_VECTOR + irq) = irq;
	return *(volatile uint8 *) (PCH_PIC_VA + PIC_VECTOR + irq) == irq ? 0
									  : -1;
}
