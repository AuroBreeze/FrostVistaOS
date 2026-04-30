#ifndef __PLATFORM_VIRTIO_MMIO_H__
#define __PLATFORM_VIRTIO_MMIO_H__

/*
 * Detail From https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.html
 */

#include "asm/machine.h"
#include "kernel/spinlock.h"

#define VIRTIO_MMIO_PHY_BASE 0x10001000
#define VIRTIO_MMIO_VIRT_BASE (VIRTIO_MMIO_PHY_BASE + KERNEL_VIRT_OFFSET)
#define VIRTIO_BLK_Q_SIZE 64
#define NUM VIRTIO_BLK_Q_SIZE

#define VIRTIO_IRQ 0x01
// device id
#define VIRTIO_BLK_ID 0x2
#define VIRTIO_ADDR(OFFSET) (VIRTIO_MMIO_VIRT_BASE + OFFSET)

#define VIRTQ_AVAIL_F_NO_INTERRUPT 1

/* This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT 1
/* This marks a buffer as device write-only (otherwise device read-only). */
#define VIRTQ_DESC_F_WRITE 2
/* This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT 4

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

// features bits
#define VIRTIO_BLK_F_SIZE_MAX                                                  \
	1 // Maximum size of any single segment is in size_max.
#define VIRTIO_BLK_F_SEG_MAX                                                   \
	2 // Maximum number of segments in a request is in seg_max.
#define VIRTIO_BLK_F_GEOMETRY 4 // Disk-style geometry specified in geometry.
#define VIRTIO_BLK_F_RO 5	// Device is read-only.
#define VIRTIO_BLK_F_BLK_SIZE 6 // Block size of disk is in blk_size.
#define VIRTIO_BLK_F_FLUSH 9	// Cache flush command support.
#define VIRTIO_BLK_F_TOPOLOGY                                                  \
	10 // Device exports information on optimal I/O alignment.
#define VIRTIO_BLK_F_CONFIG_WCE                                                \
	11 // Device can toggle its cache between writeback and writethrough
	   // modes.
#define VIRTIO_BLK_F_DISCARD                                                   \
	13 // Device can support discard command, maximum discard sectors size
	   // in max_discard_sectors and maximum discard segment number in
	   // max_discard_seg.
#define VIRTIO_BLK_F_WRITE_ZEROES                                              \
	14 // Device can support write zeroes command, maximum write zeroes
	   // sectors size in max_write_zeroes_sectors and maximum write zeroes
	   // segment number in max_write_zeroes_seg.

// virtio_blk_req type
/* The length of data MUST be a multiple of 512 bytes for VIRTIO_BLK_T_IN and
 * VIRTIO_BLK_T_OUT requests. */
#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1
#define VIRTIO_BLK_T_FLUSH 4
#define VIRTIO_BLK_T_DISCARD 11
#define VIRTIO_BLK_T_WRITE_ZEROES 13

// virtio_blk_req status
#define VIRTIO_BLK_S_OK 0
#define VIRTIO_BLK_S_IOERR 1
#define VIRTIO_BLK_S_UNSUPP 2

/* The descriptor table refers to the buffers the driver is using for the
 * device. */
/* Addr is a physical address, and the buffers can be chained via next. */
struct virtq_desc {
	uint64 addr;
	uint32 len;

	/* A driver MUST NOT set both VIRTQ_DESC_F_INDIRECT and
	 * VIRTQ_DESC_F_NEXT in flags. */
	uint16 flags;
	/* Next field if flags & NEXT */
	uint16 next;
};

struct virtq_avail {
	uint16 flags;
	/* idx field indicates where the driver would put the next descriptor
	 * entry in the ring (modulo the queue size). */
	uint16 idx;
	// uint16 ring[Queue size];
	uint16 ring[VIRTIO_BLK_Q_SIZE];
	/* Only if VIRTIO_F_EVENT_IDX*/
	uint16 used_event;
};

/* le32 is used here for ids for padding reasons. */
struct virtq_used_elem {
	/* Index of start of used descriptor chain. */
	uint32 id;
	/* Total length of the descriptor chain which was used (written to) */
	uint32 len;
};

/* The used ring is where the device returns buffers once it is done with them:
 * it is only written to by the device, and read by the driver. */
struct virtq_used {
	uint16 flags;
	uint16 idx;
	/* The driver MUST NOT make assumptions about data in device-writable
	 * buffers beyond the first len bytes, and SHOULD ignore this data. */
	struct virtq_used_elem ring[VIRTIO_BLK_Q_SIZE];
	/* Only if VIRTIO_F_EVENT_IDX */
	uint16 avail_event;
};

struct virtio_blk_geometry {
	uint16 cylinders;
	uint8 heads;
	uint8 sectors;
};

struct virtio_blk_topology {
	// # of logical blocks per physical block (log2)
	uint8 physical_block_exp;
	// offset of first aligned logical block
	uint8 alignment_offset;
	// suggested minimum I/O size in blocks
	uint16 min_io_size;
	// optimal (suggested maximum) I/O size in blocks
	uint32 opt_io_size;
};
struct virtio_blk_config {
	uint64 capacity;
	uint32 size_max;
	uint32 seg_max;
	struct virtio_blk_geometry geometry;
	uint32 blk_size;
	struct virtio_blk_topology topology;
	uint8 writeback;
	uint8 unused0[3];
	uint32 max_discard_sector;
	uint32 max_discard_seg;
	uint32 discard_sector_alignment;
	uint32 max_write_zeroes_sector;
	uint32 max_write_zeroes_seg;
	uint8 write_zeroes_may_unmap;
	uint8 unused1[3];
};

// 5.26
// Request Parameter Structure
struct virtio_blk_req {
	uint32 type;
	uint32 reserved;
	uint64 sector;
	// uint8 data[];
	// status[NUM]
	// uint8 status;
};

struct virtio_blk_discard_write_zeroes {
	uint64 sector;
	uint32 num_sectors;
	struct {
		uint32 unmap : 1;
		uint32 reserved : 31;
	} flags;
};

/*
 * +------------------+-----------+----------------------+
 * | Virtqueue Part   | Alignment | Size                 |
 * +------------------+-----------+----------------------+
 * | Descriptor Table | 16        | 16 * (Queue Size)    |
 * | Available Ring   | 2         | 6 + 2 * (Queue Size) |
 * | Used Ring        | 4         | 6 + 8 * (Queue Size) |
 * +------------------+-----------+----------------------+
 */
struct Virtqueue {
	// After allocating the space, use it as an array
	struct virtq_desc *desc;
	struct virtq_avail *avail; // Driver write
	struct virtq_used *used;   // Device write
};

struct VirtioBlkDrvier {
	struct Virtqueue vq;
	// Stores the state of requests received and processed by virtio, as
	// well as the currently active bcache, for the purpose of resuming
	struct virtio_blk_req req[NUM];
	// Stores the state of requests received and processed by virtio, as
	// well as the currently active bcache, for the purpose of resuming
	struct {
		struct buf *buffer;
		char status;
	} info[NUM];

	uint16 last_used_idx;
	int free_desc_idx; // This index is empty

	struct spinlock blk_lock;
};

#endif
