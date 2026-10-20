#ifndef FV_DRIVER_VIRTIO_TRANSPORT_H
#define FV_DRIVER_VIRTIO_TRANSPORT_H

#include "kernel/types.h"

struct virtio_device;

struct virtio_transport_ops {
	/* Probe the requested VirtIO device ID and detect legacy/modern. */
	int (*probe_device)(struct virtio_device *dev, uint32 device_id);
	int (*reset)(struct virtio_device *dev);

	/* Status values use 8 bits; backends preserve hardware access widths.
	 */
	uint8 (*get_status)(struct virtio_device *dev);
	void (*set_status)(struct virtio_device *dev, uint8 status);

	uint64 (*get_features)(struct virtio_device *dev);
	void (*set_features)(struct virtio_device *dev, uint64 features);

	uint8 (*get_queue_ready)(struct virtio_device *dev, uint16 queue);
	uint32 (*get_queue_max_size)(struct virtio_device *dev, uint16 queue);
	/* Addresses are already physical; the backend must not convert again.
	 * The caller initializes queue memory before setup enables the queue.
	 * Legacy MMIO requires a contiguous, page-aligned split-ring layout. */
	int (*setup_queue)(struct virtio_device *dev, uint16 queue, uint16 size,
			   uint64 desc_pa, uint64 avail_pa, uint64 used_pa);

	void (*notify_queue)(struct virtio_device *dev, uint16 queue);
	uint8 (*ack_interrupt)(struct virtio_device *dev);

	/* Read one naturally aligned 1/2/4/8-byte device-specific field. */
	int (*read_config)(struct virtio_device *dev, uint32 offset,
			   void *buffer, uint32 length);
};

struct virtio_device {
	const struct virtio_transport_ops *ops;
	void *transport_data;
	int modern;
};

extern struct virtio_device virtio_blk_device;
extern const struct virtio_transport_ops virtio_blk_device_ops;

#endif
