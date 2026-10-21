#define LOG_MODULE "EIOINTC"

#include <asm/loongarch.h>
#include <kernel/defs.h>
#include <kernel/irq.h>
#include <kernel/log.h>
#include <platform/eiointc.h>

/* Absolute IOCSR addresses, accessed through IOCSR instructions. */
/* One routing byte per 32 vectors; bitmap 0x02 selects HWI1. */
#define EIO_IPMAP 0x14c0U
/* Enable bitmap for 256 vectors: 1 = enabled, 0 = disabled. */
#define EIO_ENABLE 0x1600U
/* Bounce configuration bitmap; kept zero for fixed CPU 0 routing. */
#define EIO_BOUNCE 0x1680U
/* Current CPU's pending bitmap; write 1 to clear the corresponding bit. */
#define EIO_COREISR 0x1800U
/* One CPU routing byte per vector: CPU 0 is 0x01 in bitmap mode,
 * or 0x00 when CPU-number encoding is enabled. */
#define EIO_COREMAP 0x1c00U
/* QEMU virtual-extension capabilities: bit 0 = extension present,
 * bit 1 = programmable enable, bit 3 = CPU-number encoding supported. */
#define EIO_VIRT_FEATURES 0x40000000U
/* Virtual-extension control: bit 1 = enable, bit 2 = encoded IP routing,
 * bit 3 = encoded CPU routing. Encoded IP routing is unsupported here. */
#define EIO_VIRT_CONFIG 0x40000004U
/* Miscellaneous IOCSR register; bit 48 is the EIOINTC enable bit. */
#define EIO_MISC 0x420U

static int initialized;
static uint8 cpu0_route;

static void eiointc_write_route_byte(uint32 addr, uint8 value)
{
	uint32 aligned = addr & ~3U;
	uint32 shift = (addr & 3U) * 8;
	uint32 old = iocsr_read32(aligned);
	iocsr_write32(aligned,
		      (old & ~(0xffU << shift)) | ((uint32) value << shift));
}

int eiointc_init(void)
{
	if (initialized)
		return 0;
	/* This bring-up routes all registered external IRQs to CPU 0. */
	if (cpuid() != 0)
		return -1;
	uint32 features = iocsr_read32(EIO_VIRT_FEATURES);
	uint32 config = 0;
	int virtual = features != 0xffffffffU && (features & 1U);
	if (virtual) {
		config = iocsr_read32(EIO_VIRT_CONFIG);
		/* IPMAP below uses bitmap encoding, not encoded pin numbers. */
		if (config & (1U << 2)) {
			LOG_ERROR(
			    "Encoded EIOINTC interrupt pins are unsupported");
			return -1;
		}
	}
	cpu0_route = (config & (1U << 3)) ? 0 : 1;
	for (uint32 group = 0; group < IRQ_VECTOR_COUNT / 32; group++) {
		iocsr_write32(EIO_ENABLE + (group * 4), 0);
		iocsr_write32(EIO_BOUNCE + (group * 4), 0);
		iocsr_write32(EIO_COREISR + (group * 4), 0xffffffffU);
	}
	if (virtual && (features & (1U << 1))) {
		iocsr_write32(EIO_VIRT_CONFIG, config | (1U << 1));
		if (!(iocsr_read32(EIO_VIRT_CONFIG) & (1U << 1)))
			return -1;
	} else {
		uint64 misc = iocsr_read64(EIO_MISC);
		if (!(misc & (1ULL << 48))) {
			iocsr_write64(EIO_MISC, misc | (1ULL << 48));
			if (!(iocsr_read64(EIO_MISC) & (1ULL << 48)))
				return -1;
		}
	}
	initialized = 1;
	LOG_INFO("EIOINTC ready: CPU 0, HWI1, virtual=%d cpu-encoded=%d",
		 virtual, cpu0_route == 0);
	return 0;
}

void eiointc_disable_vector(uint32 vector)
{
	if (vector >= IRQ_VECTOR_COUNT)
		return;
	uint32 addr = EIO_ENABLE + ((vector / 32) * 4);
	iocsr_write32(addr, iocsr_read32(addr) & ~(1U << (vector % 32)));
}

void eiointc_enable_vector(uint32 vector)
{
	if (vector >= IRQ_VECTOR_COUNT)
		return;
	uint32 addr = EIO_ENABLE + ((vector / 32) * 4);
	iocsr_write32(addr, iocsr_read32(addr) | (1U << (vector % 32)));
}

void eiointc_ack_vector(uint32 vector)
{
	if (vector < IRQ_VECTOR_COUNT)
		iocsr_write32(EIO_COREISR + ((vector / 32) * 4),
			      1U << (vector % 32));
}

uint32 eiointc_pending(uint32 group)
{
	return group < IRQ_VECTOR_COUNT / 32
		   ? iocsr_read32(EIO_COREISR + (group * 4))
		   : 0;
}

int eiointc_configure_vector(uint32 vector)
{
	if (!initialized || vector >= IRQ_VECTOR_COUNT)
		return -1;
	eiointc_disable_vector(vector);
	eiointc_write_route_byte(EIO_COREMAP + vector, cpu0_route);
	/* All vectors in this group share the same HWI1 output. */
	eiointc_write_route_byte(EIO_IPMAP + (vector / 32), 0x02);
	eiointc_ack_vector(vector);
	return 0;
}
