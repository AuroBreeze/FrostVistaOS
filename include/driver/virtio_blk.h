#ifndef __DRIVER_VIRTIO_BLK_H__
#define __DRIVER_VIRTIO_BLK_H__

#include "driver/virtio.h"
#include "kernel/spinlock.h"
#include "kernel/types.h"

#define VIRTIO_BLK_Q_SIZE 64
#define NUM VIRTIO_BLK_Q_SIZE

// device id
#define VIRTIO_BLK_ID 0x2

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

// virtio_blk_req type.
#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1
#define VIRTIO_BLK_T_FLUSH 4
#define VIRTIO_BLK_T_DISCARD 11
#define VIRTIO_BLK_T_WRITE_ZEROES 13

// virtio_blk_req status.
#define VIRTIO_BLK_S_OK 0
#define VIRTIO_BLK_S_IOERR 1
#define VIRTIO_BLK_S_UNSUPP 2

struct virtq_desc {
	/* The descriptor table refers to the buffers the driver is using for
	 * the device. */
	/* Addr is a physical address, and the buffers can be chained via next.
	 */
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
	uint16 idx;
	/* idx field indicates where the driver would put the next descriptor
	 * entry in the ring (modulo the queue size). */
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

struct virtq_used {
	uint16 flags;
	uint16 idx;
	/* The used ring is where the device returns buffers once it is done
	 * with them: it is only written to by the device, and read by the
	 * driver. */
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
	// Maximum number of segments in a request.
	uint32 seg_max;
	struct virtio_blk_geometry geometry;
	// Block size of disk.
	uint32 blk_size;
	struct virtio_blk_topology topology;
	// Device writeback mode.
	uint8 writeback;
	// Reserved configuration bytes.
	uint8 unused0[3];
	// Maximum discard sectors and segments.
	uint32 max_discard_sector;
	uint32 max_discard_seg;
	// Discard sector alignment.
	uint32 discard_sector_alignment;
	// Maximum write-zeroes sectors and segments.
	uint32 max_write_zeroes_sector;
	uint32 max_write_zeroes_seg;
	// Whether write-zeroes may unmap.
	uint8 write_zeroes_may_unmap;
	// Reserved configuration bytes.
	uint8 unused1[3];
};

// 5.26
// Request Parameter Structure
struct virtio_blk_req {
	uint32 type;
	uint32 reserved;
	uint64 sector;
};

struct virtio_blk_discard_write_zeroes {
	uint64 sector;
	uint32 num_sectors;
	struct {
		// Unmap the sectors instead of writing zeroes.
		uint32 unmap : 1;
		uint32 reserved : 31;
	} flags;
};

struct Virtqueue {
	/*
	 * +------------------+-----------+----------------------+
	 * | Virtqueue Part   | Alignment | Size                 |
	 * +------------------+-----------+----------------------+
	 * | Descriptor Table | 16        | 16 * (Queue Size)    |
	 * | Available Ring   | 2         | 6 + 2 * (Queue Size) |
	 * | Used Ring        | 4         | 6 + 8 * (Queue Size) |
	 * +------------------+-----------+----------------------+
	 */
	// After allocating the space, use it as an array
	struct virtq_desc *desc;
	struct virtq_avail *avail; // Driver write
	struct virtq_used *used;   // Device write
};

struct buf;

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
