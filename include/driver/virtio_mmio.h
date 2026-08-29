#ifndef __DRIVER_VIRTIO_MMIO_H__
#define __DRIVER_VIRTIO_MMIO_H__

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

// Offset Path
// W=writable R=readable, if not write W, it will be read
#define VIRTIO_MAGIC_VALUE 0x000 // magic value 0x74726976
#define VIRTIO_VERSION 0x004	 // version 0x1
#define VIRTIO_DEVICE_ID 0x008	 // must miss 0x0

#define VIRTIO_DEVICE_FEATURES                                                 \
	0x010 // will return bits DeviceFeaturesSel ∗ 32 to (DeviceFeaturesSel ∗
	      // 32) + 31, eg. feature bits 0 to 31 if DeviceFeaturesSel is set
	      // to 0 and features bits 32 to 63 if DeviceFeaturesSel is set
	      // to 1.
#define VIRTIO_DEVICE_FEATURES_SEL 0x014 // W

#define VIRTIO_DRIVER_FEATURES 0x020	 // W
#define VIRTIO_DRIVER_FEATURES_SEL 0x024 // W

#define VIRTIO_QUEUE_SELECT                                                    \
	0x030 // W Writing to this register selects the virtual queue that the
	      // following operations on QueueNumMax, QueueNum, QueueReady,
	      // QueueDescLow, QueueDescHigh, QueueAvailLow, QueueAvailHigh,
	      // QueueUsedLow and QueueUsedHigh apply to. The index number of
	      // the first queue is zero (0x0).
#define VIRTIO_QUEUE_NUM_MAX                                                   \
	0x034 // Reading from the register returns the maximum size (number of
	      // elements) of the queue the device is ready to process or zero
	      // (0x0) if the queue is not available. This applies to the queue
	      // selected by writing to QueueSel.
#define VIRTIO_QUEUE_NUM                                                       \
	0x038 // W Queue size is the number of elements in the queue. Writing to
	      // this register notifies the device what size of the queue the
	      // driver will use.

#define VIRTIO_GUEST_PAGE_SIZE 0x028 // legacy
#define VIRTIO_QUEUE_ALIGN 0x03c     // legacy
#define VIRTIO_QUEUE_PFN 0x040	     // legacy

#define VIRTIO_QUEUE_READY                                                     \
	0x044 // RW Writing one (0x1) to this register notifies the device that
	      // it can execute requests from this virtual queue. Reading from
	      // this register returns the last value written to it. Both read
	      // and write accesses apply to the queue selected by writing to
	      // QueueSel.

#define VIRTIO_QUEUE_NOTIFY 0x050 // W

#define VIRTIO_INTERRUPT_STATUS 0x60
#define VIRTIO_INTERRUPT_ACK                                                   \
	0x064 // W Writing a value with bits set as defined in InterruptStatus
	      // to this register notifies the device that events causing the
	      // interrupt have been handled.

#define VIRTIO_STATUS 0x070 // RW

/* Virtual queue’s Descriptor Area 64 bit long physical address */
#define VIRTIO_QUEUE_DESC_LOW 0x080  // W
#define VIRTIO_QUEUE_DESC_HIGH 0x084 // W

/* Virtual queue’s Driver Area 64 bit long physical address */
#define VIRTIO_QUEUE_DRIVER_LOW 0x090  // W
#define VIRTIO_QUEUE_DRIVER_HIGH 0x094 // W

/* Virtual queue’s Device Area 64 bit long physical address */
#define VIRTIO_QUEUE_DEVICE_LOW 0x0a0  // W
#define VIRTIO_QUEUE_DEVICE_HIGH 0x0a4 // W

#define VIRTIO_CONFIG_GENERATION 0x0fc // R

#endif
