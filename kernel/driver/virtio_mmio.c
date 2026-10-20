#ifdef VIRTIO_MMIO_TRANSPORT

#define LOG_MODULE "VIRTIO_MMIO"

#include "kernel/log.h"
#include "kernel/arch/mm.h"
#include "driver/virtio_transport.h"
#include "driver/virtio_mmio.h"
#include "platform/virtio_mmio.h"

#define VIRTIO_MMIO_CONFIG_BASE 0x100U

static int virtio_mmio_probe_device(struct virtio_device *dev, uint32 device_id)
{
	uint32 magic = VIRTIO_READ32(VIRTIO_MAGIC_VALUE);
	uint32 version = VIRTIO_READ32(VIRTIO_VERSION);
	uint32 found_id = VIRTIO_READ32(VIRTIO_DEVICE_ID);
	dev->modern = 0;
	if (magic != 0x74726976 || (version != 1 && version != 2) ||
	    found_id != device_id) {
		LOG_ERROR("MMIO probe failed: magic=0x%x version=%d device=%d",
			  magic, version, found_id);
		return -1;
	}
	dev->modern = (version == 2);
	return 0;
}

static uint8 virtio_mmio_get_status(struct virtio_device *dev)
{
	(void) dev;
	/* MMIO control accesses are 32-bit, even for an 8-bit status value. */
	return (uint8) VIRTIO_READ32(VIRTIO_STATUS);
}

static void virtio_mmio_set_status(struct virtio_device *dev, uint8 status)
{
	(void) dev;
	VIRTIO_WRITE32(VIRTIO_STATUS, status);
}

static int virtio_mmio_reset(struct virtio_device *dev)
{
	virtio_mmio_set_status(dev, 0);
	for (uint32 i = 0; i < VIRTIO_MMIO_POLL_LIMIT; i++) {
		if (VIRTIO_READ32(VIRTIO_STATUS) == 0)
			return 0;
	}
	LOG_ERROR("MMIO reset did not complete");
	return -1;
}

static uint64 virtio_mmio_get_features(struct virtio_device *dev)
{
	VIRTIO_WRITE32(VIRTIO_DEVICE_FEATURES_SEL, 0);
	uint64 features = VIRTIO_READ32(VIRTIO_DEVICE_FEATURES);
	if (dev->modern) {
		VIRTIO_WRITE32(VIRTIO_DEVICE_FEATURES_SEL, 1);
		features |= (uint64) VIRTIO_READ32(VIRTIO_DEVICE_FEATURES)
			    << 32;
	}
	return features;
}

static void virtio_mmio_set_features(struct virtio_device *dev, uint64 features)
{
	VIRTIO_WRITE32(VIRTIO_DRIVER_FEATURES_SEL, 0);
	VIRTIO_WRITE32(VIRTIO_DRIVER_FEATURES, (uint32) features);
	if (dev->modern) {
		VIRTIO_WRITE32(VIRTIO_DRIVER_FEATURES_SEL, 1);
		VIRTIO_WRITE32(VIRTIO_DRIVER_FEATURES,
			       (uint32) (features >> 32));
	}
}

static uint8 virtio_mmio_queue_ready(struct virtio_device *dev, uint16 queue)
{
	VIRTIO_WRITE32(VIRTIO_QUEUE_SELECT, queue);
	/* QueueReady does not exist in the legacy register layout. */
	if (!dev->modern)
		return VIRTIO_READ32(VIRTIO_QUEUE_PFN) != 0;
	return VIRTIO_READ32(VIRTIO_QUEUE_READY) != 0;
}

static uint32 virtio_mmio_get_queue_max_size(struct virtio_device *dev,
					     uint16 queue)
{
	(void) dev;
	VIRTIO_WRITE32(VIRTIO_QUEUE_SELECT, queue);
	return VIRTIO_READ32(VIRTIO_QUEUE_NUM_MAX);
}

static int virtio_mmio_setup_queue(struct virtio_device *dev, uint16 queue,
				   uint16 size, uint64 desc_pa, uint64 avail_pa,
				   uint64 used_pa)
{
	if (!size || (size & (size - 1)) || size > 32768 || (desc_pa & 15U) ||
	    (avail_pa & 1U) || (used_pa & 3U) ||
	    virtio_mmio_queue_ready(dev, queue) ||
	    virtio_mmio_get_queue_max_size(dev, queue) < size)
		return -1;

	if (!dev->modern) {
		/* Legacy PFN describes one contiguous allocation. */
		uint64 expected_avail = desc_pa + (16ULL * size);
		uint64 expected_used =
		    PGROUNDUP(expected_avail + 6ULL + 2ULL * size);
		if ((desc_pa & (PGSIZE - 1)) || !desc_pa ||
		    (desc_pa >> 12) > 0xffffffffULL ||
		    avail_pa != expected_avail || used_pa != expected_used)
			return -1;
		VIRTIO_WRITE32(VIRTIO_GUEST_PAGE_SIZE, PGSIZE);
		VIRTIO_WRITE32(VIRTIO_QUEUE_NUM, size);
		VIRTIO_WRITE32(VIRTIO_QUEUE_ALIGN, PGSIZE);
		__sync_synchronize();
		VIRTIO_WRITE32(VIRTIO_QUEUE_PFN, (uint32) (desc_pa >> 12));
		return VIRTIO_READ32(VIRTIO_QUEUE_PFN) == (desc_pa >> 12) ? 0
									  : -1;
	}

	/* The caller already converted the three addresses to physical. */
	VIRTIO_WRITE32(VIRTIO_QUEUE_NUM, size);
	VIRTIO_WRITE32(VIRTIO_QUEUE_DESC_LOW, (uint32) desc_pa);
	VIRTIO_WRITE32(VIRTIO_QUEUE_DESC_HIGH, (uint32) (desc_pa >> 32));
	VIRTIO_WRITE32(VIRTIO_QUEUE_DRIVER_LOW, (uint32) avail_pa);
	VIRTIO_WRITE32(VIRTIO_QUEUE_DRIVER_HIGH, (uint32) (avail_pa >> 32));
	VIRTIO_WRITE32(VIRTIO_QUEUE_DEVICE_LOW, (uint32) used_pa);
	VIRTIO_WRITE32(VIRTIO_QUEUE_DEVICE_HIGH, (uint32) (used_pa >> 32));
	__sync_synchronize();
	VIRTIO_WRITE32(VIRTIO_QUEUE_READY, 1);
	return VIRTIO_READ32(VIRTIO_QUEUE_READY) == 1 ? 0 : -1;
}

static void virtio_mmio_notify_queue(struct virtio_device *dev, uint16 queue)
{
	(void) dev;
	VIRTIO_WRITE32(VIRTIO_QUEUE_NOTIFY, queue);
}

static uint8 virtio_mmio_ack_interrupt(struct virtio_device *dev)
{
	(void) dev;
	uint32 reason = VIRTIO_READ32(VIRTIO_INTERRUPT_STATUS) & 3U;
	VIRTIO_WRITE32(VIRTIO_INTERRUPT_ACK, reason);
	return (uint8) reason;
}

/* Read one naturally aligned field. 64-bit fields use two 32-bit accesses. */
static int virtio_mmio_read_config(struct virtio_device *dev, uint32 offset,
				   void *buffer, uint32 length)
{
	if (!buffer ||
	    (length != 1 && length != 2 && length != 4 && length != 8) ||
	    (offset & ((length < 4 ? length : 4) - 1)) ||
	    offset > PGSIZE - VIRTIO_MMIO_CONFIG_BASE - length)
		return -1;
	uint64 addr = VIRTIO_ADDR(VIRTIO_MMIO_CONFIG_BASE + offset);
	for (uint32 i = 0; i < VIRTIO_MMIO_POLL_LIMIT; i++) {
		uint32 generation =
		    dev->modern ? VIRTIO_READ32(VIRTIO_CONFIG_GENERATION) : 0;
		__sync_synchronize();
		uint64 value;
		if (length == 1)
			value = *(volatile uint8 *) addr;
		else if (length == 2)
			value = *(volatile uint16 *) addr;
		else {
			value = *(volatile uint32 *) addr;
			if (length == 8)
				value |=
				    (uint64) * (volatile uint32 *) (addr + 4)
				    << 32;
		}
		__sync_synchronize();
		if (dev->modern &&
		    generation != VIRTIO_READ32(VIRTIO_CONFIG_GENERATION))
			continue;
		/* Copy bytes so the caller's output buffer need not be aligned.
		 */
		for (uint32 j = 0; j < length; j++)
			((uint8 *) buffer)[j] = (uint8) (value >> (8 * j));
		return 0;
	}
	return -1;
}

const struct virtio_transport_ops virtio_blk_device_ops = {
    .probe_device = virtio_mmio_probe_device,
    .reset = virtio_mmio_reset,
    .get_status = virtio_mmio_get_status,
    .set_status = virtio_mmio_set_status,
    .get_features = virtio_mmio_get_features,
    .set_features = virtio_mmio_set_features,
    .get_queue_ready = virtio_mmio_queue_ready,
    .get_queue_max_size = virtio_mmio_get_queue_max_size,
    .setup_queue = virtio_mmio_setup_queue,
    .notify_queue = virtio_mmio_notify_queue,
    .ack_interrupt = virtio_mmio_ack_interrupt,
    .read_config = virtio_mmio_read_config,
};

struct virtio_device virtio_blk_device = {
    .ops = &virtio_blk_device_ops,
    .transport_data = 0,
    .pci_dev = 0,
    .modern = 0,
};
#endif
