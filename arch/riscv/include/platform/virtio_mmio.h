#ifndef __PLATFORM_VIRTIO_MMIO_H__
#define __PLATFORM_VIRTIO_MMIO_H__

#include "asm/machine.h"
#include "driver/virtio_mmio.h"

#define VIRTIO_MMIO_PHY_BASE 0x10001000
#define VIRTIO_MMIO_VIRT_BASE (VIRTIO_MMIO_PHY_BASE + KERNEL_VIRT_OFFSET)

#define VIRTIO_IRQ 0x01
#define VIRTIO_ADDR(offset) (VIRTIO_MMIO_VIRT_BASE + (offset))
#define VIRTIO_READ32(offset) (*(volatile uint32 *) VIRTIO_ADDR(offset))
#define VIRTIO_WRITE32(offset, value)                                          \
	(*(volatile uint32 *) VIRTIO_ADDR(offset) = (uint32) (value))

#endif
