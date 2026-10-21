#ifndef FV_DRIVER_VIRTIO_TRANSPORT_H
#define FV_DRIVER_VIRTIO_TRANSPORT_H

#include "driver/pci.h"
#include "kernel/types.h"
#include "kernel/irq.h"

#define VIRTIO_MMIO_POLL_LIMIT 1000000U

/*
 * Detail From https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.html
 */

// 2.1 Device Status Field
#define VIRTIO_CONFIG_S_ACKNOWLEDGE                                            \
	1 // Indicates that the guest OS has found the device and recognized it
	  // as a valid virtio device.
#define VIRTIO_CONFIG_S_DRIVER                                                 \
	2 // Indicates that the guest OS knows how to drive the device. Note:
	  // There could be a significant (or infinite) delay before setting
	  // this bit. For example, under Linux, drivers can be loadable
	  // modules.
#define VIRTIO_CONFIG_S_FAILED                                                 \
	128 // Indicates that something went wrong in the guest, and it has
	    // given up on the device. This could be an internal error, or the
	    // driver didn’t like the device for some reason, or even a fatal
	    // error during device operation.
#define VIRTIO_CONFIG_S_FEATURES_OK                                            \
	8 // Indicates that the driver has acknowledged all the features it
	  // understands, and feature negotiation is complete.
#define VIRTIO_CONFIG_S_DRIVER_OK                                              \
	4 // Indicates that the driver is set up and ready to drive the device.
#define VIRTIO_CONFIG_S_DEVICE_NEEDS_RESET                                     \
	64 // Indicates that the device has experienced an error from which it
	   // can’t recover.

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
	/* Optional for transports with a platform-owned IRQ registration path.
	 */
	int (*register_irq)(struct virtio_device *dev, irq_handler_t handler,
			    void *arg);

	/* Read one naturally aligned 1/2/4/8-byte device-specific field. */
	int (*read_config)(struct virtio_device *dev, uint32 offset,
			   void *buffer, uint32 length);
};

struct virtio_device {
	const struct virtio_transport_ops *ops;
	const struct pci_device *pci_dev;
	void *transport_data;
	int modern;
};

extern struct virtio_device virtio_blk_device;
extern const struct virtio_transport_ops virtio_blk_device_ops;

#endif
