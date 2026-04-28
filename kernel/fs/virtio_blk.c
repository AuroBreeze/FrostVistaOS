#include "asm/mm.h"
#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "platform/virtio_mmio.h"

struct VirtioBlkDrvier driver;

void init_free_desc_list() {
  for (int i = 0; i < NUM; i++) {
    driver.vq.desc[i].next = i + 1;
  }

  driver.vq.desc[NUM - 1].next = 0xff;
  driver.free_desc_idx = 0;
}

/**
 * alloc_desc - Allocate a free descriptor
 * */
int alloc_desc() {
  if (driver.free_desc_idx == 0xff) {
    return -1;
  }

  int idx = driver.free_desc_idx;
  driver.free_desc_idx = driver.vq.desc[idx].next;
  return idx;
}

int free_desc(int idx) {
  if (idx < 0 || idx >= NUM) {
    LOG_ERROR("free_desc idx out of range");
    return -1;
  }
  driver.vq.desc[idx].addr = 0;
  driver.vq.desc[idx].len = 0;
  driver.vq.desc[idx].flags = 0;
  driver.vq.desc[idx].next = 0;
  return 0;
}

void insert_free_desc_list(int idx) {
  if (free_desc(idx)) {
    return;
  }
  int current = driver.free_desc_idx;
  driver.free_desc_idx = idx;
  driver.vq.desc[idx].next = current;
  return;
}

void free_chain(int idx) {
  if (idx < 0 || idx >= NUM) {
    LOG_ERROR("free_desc idx out of range");
    return;
  }
  while (idx != 0xff) {
    int next = driver.vq.desc[idx].next;
    free_desc(idx);
    insert_free_desc_list(idx);
    idx = next;
  }
}

// xv6
// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
static int alloc3_desc(int *idx) {
  for (int i = 0; i < 3; i++) {
    idx[i] = alloc_desc();
    if (idx[i] < 0) {
      for (int j = 0; j < i; j++)
        free_desc(idx[j]);
      return -1;
    }
  }
  return 0;
}

/**
 * virtio_disk_rw - Read/Write block
 * */
void virtio_disk_rw(struct buf *buffer, int write) {

  acquire(&driver.blk_lock);

  int idx[3];
  while (1) {
    if (alloc3_desc(idx) == 0) {
      break;
    }
    sleep(&driver.free_desc_idx, &driver.blk_lock);
  }

  struct virtio_blk_req *req = &driver.req[idx[0]];
  if (write) {
    req->type = VIRTIO_BLK_T_OUT; // write to disk
  } else {
    req->type = VIRTIO_BLK_T_IN; // read from disk
  }

  req->sector = buffer->blkno * (BSIZE / 512);
  req->reserved = 0;

  // Set block device requests
  driver.vq.desc[idx[0]].addr = VA2PA(req);
  driver.vq.desc[idx[0]].len = sizeof(struct virtio_blk_req);
  driver.vq.desc[idx[0]].flags = VIRTQ_DESC_F_NEXT;
  driver.vq.desc[idx[0]].next = idx[1];

  // The buffer to be written to or read from
  driver.vq.desc[idx[1]].addr = VA2PA(buffer->data);
  driver.vq.desc[idx[1]].len = BSIZE;
  if (write) {
    driver.vq.desc[idx[1]].flags = 0; // Data flows from `buf->data` to the disk
  } else {
    driver.vq.desc[idx[1]].flags =
        VIRTQ_DESC_F_WRITE; // The disk loads the data into `buf->data`
  }
  driver.vq.desc[idx[1]].flags |= VIRTQ_DESC_F_NEXT;
  driver.vq.desc[idx[1]].next = idx[2];

  // virtio_blk_req.status
  // Split into separate arrays
  driver.vq.desc[idx[2]].addr = VA2PA(&driver.info[idx[0]].status);
  driver.vq.desc[idx[2]].len = 1;
  driver.vq.desc[idx[2]].flags = VIRTQ_DESC_F_WRITE;
  driver.vq.desc[idx[2]].next = 0;

  buffer->done = 0;
  driver.info[idx[0]].buffer = buffer;

  // tell the device the first index in our chain of descriptors.
  driver.vq.avail->ring[driver.vq.avail->idx % NUM] = idx[0];

  __sync_synchronize();

  driver.vq.avail->idx += 1;

  __sync_synchronize();

  // Writing a value to this register notifies the device that there are new
  // buffers to process in a queue.
  *(uint32 *)VIRTIO_ADDR(VIRTIO_QUEUE_NOTIFY) = 0;

  while (!buffer->done) {
    sleep(buffer, &driver.blk_lock);
  }

  driver.info[idx[0]].buffer = 0;
  free_chain(idx[0]);

  release(&driver.blk_lock);
}

/**
 * virtio_disk_init - Initialize virtio disk
 * */
void virtio_disk_init() {
  uint32 *p = (uint32 *)(VIRTIO_ADDR(VIRTIO_MAGIC_VALUE));

  uint32 magic = *p;

  p = (uint32 *)(VIRTIO_ADDR(VIRTIO_VERSION));
  uint32 version = *p;
  if (magic != 0x74726976 || version != 0x2) {
    LOG_DEBUG("virtio_blk test failed, magic 0x%x, version 0x%x", magic,
              version);
    return;
  }

  int status = *(uint32 *)VIRTIO_ADDR(VIRTIO_STATUS);
  // Reset device
  status = 0;
  *(uint32 *)VIRTIO_ADDR(VIRTIO_STATUS) = status;
  // Set ACKNOWLEDGE
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *(uint32 *)VIRTIO_ADDR(VIRTIO_STATUS) = status;
  // Set DRIVER
  status |= VIRTIO_CONFIG_S_DRIVER;
  *(uint32 *)VIRTIO_ADDR(VIRTIO_STATUS) = status;
  // Set features
  // Set features page 0 that 0~31
  *(uint32 *)VIRTIO_ADDR(VIRTIO_DEVICE_FEATURES_SEL) = 0;
  uint32 features = *(uint32 *)VIRTIO_ADDR(VIRTIO_DEVICE_FEATURES);

  features &= ~((1 << VIRTIO_BLK_F_SIZE_MAX) | (1 << VIRTIO_BLK_F_SEG_MAX) |
                (1 << VIRTIO_BLK_F_GEOMETRY) | (1 << VIRTIO_BLK_F_RO) |
                (1 << VIRTIO_BLK_F_BLK_SIZE) | (1 << VIRTIO_BLK_F_FLUSH) |
                (1 << VIRTIO_BLK_F_TOPOLOGY) | (1 << VIRTIO_BLK_F_CONFIG_WCE) |
                (1 << VIRTIO_BLK_F_DISCARD) | (1 << VIRTIO_BLK_F_WRITE_ZEROES));

  // Set features page 0 that 0~31
  *(uint32 *)VIRTIO_ADDR(VIRTIO_DRIVER_FEATURES_SEL) = 0;
  *(uint32 *)VIRTIO_ADDR(VIRTIO_DRIVER_FEATURES) = features;

  // Set FEATURES_OK
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *(uint32 *)VIRTIO_ADDR(VIRTIO_STATUS) = status;

  // Re-read and check
  status = *(uint32 *)VIRTIO_ADDR(VIRTIO_STATUS);
  if (!(status & VIRTIO_CONFIG_S_FEATURES_OK)) {
    panic("virtio_blk test failed, FEATURES_OK is not set");
  }

  // Select QueueNumMax
  *(uint32 *)VIRTIO_ADDR(VIRTIO_QUEUE_SELECT) = 0;

  // When QueueReady is not zero, the driver MUST NOT access QueueNum,
  // QueueDescLow, QueueDescHigh, QueueAvailLow, QueueAvailHigh, QueueUsedLow,
  // QueueUsedHigh.
  if (*(uint32 *)VIRTIO_ADDR(VIRTIO_QUEUE_READY)) {
    panic("virtio_blk test failed, QueueReady is not zero");
  }
  // check QueueNumMax, if less than VIRTIO_BLK_Q_SIZE(8), panic
  uint32 queue_num_max = *(uint32 *)VIRTIO_ADDR(VIRTIO_QUEUE_NUM_MAX);
  if (queue_num_max < VIRTIO_BLK_Q_SIZE) {
    panic("virtio_blk test failed, QueueNumMax is less than %d",
          VIRTIO_BLK_Q_SIZE);
  }
  LOG_TRACE("QueueNumMax: %d", queue_num_max);

  // Write QueueNum
  *(uint32 *)VIRTIO_ADDR(VIRTIO_QUEUE_NUM) = VIRTIO_BLK_Q_SIZE;

  // NOTE: kalloc allocates a virtual address
  driver.vq.avail = kalloc();
  driver.vq.used = kalloc();
  driver.vq.desc = kalloc();

  if (!driver.vq.avail || !driver.vq.used || !driver.vq.desc) {
    panic("virtio_blk test failed, alloc failed");
  }

  uint64 desc_addr = VA2PA(driver.vq.desc);
  uint64 avail_addr = VA2PA(driver.vq.avail);
  uint64 used_addr = VA2PA(driver.vq.used);

  uint32 desc_low = desc_addr & 0xFFFFFFFF;
  uint32 desc_high = desc_addr >> 32;
  uint32 avail_low = avail_addr & 0xFFFFFFFF;
  uint32 avail_high = avail_addr >> 32;
  uint32 used_low = used_addr & 0xFFFFFFFF;
  uint32 used_high = used_addr >> 32;
  // Note: Note that previous versions of this spec used different names for
  // these parts (following 2.6):
  // 1. Descriptor Table - for the Descriptor Area
  // 2. Available Ring - for the Driver Area
  // 3. Used Ring - for the Device Area

  *(uint32 *)VIRTIO_ADDR(VIRTIO_QUEUE_DESC_LOW) = desc_low;
  *(uint32 *)VIRTIO_ADDR(VIRTIO_QUEUE_DESC_HIGH) = desc_high;
  *(uint32 *)VIRTIO_ADDR(VIRTIO_QUEUE_DRIVER_LOW) = avail_low;
  *(uint32 *)VIRTIO_ADDR(VIRTIO_QUEUE_DRIVER_HIGH) = avail_high;
  *(uint32 *)VIRTIO_ADDR(VIRTIO_QUEUE_DEVICE_LOW) = used_low;
  *(uint32 *)VIRTIO_ADDR(VIRTIO_QUEUE_DEVICE_HIGH) = used_high;

  // Initialize free list
  init_free_desc_list();

  // Writing one (0x1) to this register notifies the device that it can execute
  // requests from this virtual queue.
  *(uint32 *)VIRTIO_ADDR(VIRTIO_QUEUE_READY) = 1;

  // At this point the device is “live”.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *(uint32 *)VIRTIO_ADDR(VIRTIO_STATUS) = status;

  LOG_DEBUG("virtio_blk test passed");
}

void virtio_disk_intr() {
  acquire(&driver.blk_lock);

  *(uint32 *)VIRTIO_ADDR(VIRTIO_INTERRUPT_ACK) =
      *(uint32 *)VIRTIO_ADDR(VIRTIO_INTERRUPT_STATUS) & 0x3;
  __sync_synchronize();

  // The Golden Rule of Ring Buffers:
  // Compare OUR software bookmark (last_used_idx) with the HARDWARE's index
  // (used->idx)
  while (driver.last_used_idx != driver.vq.used->idx) {

    __sync_synchronize();

    // Read the ID of the completed descriptor from the used ring
    int id = driver.vq.used->ring[driver.last_used_idx % NUM].id;

    // Check the status byte written by the device
    if (driver.info[id].status != 0) {
      panic("virtio_disk_intr status error");
    }

    // Retrieve the buffer waiting for this I/O
    struct buf *buffer = driver.info[id].buffer;
    buffer->done = 1;

    // Wake up the specific process sleeping on this buffer's 'done' flag
    wakeup(buffer);

    driver.last_used_idx++;
  }

  release(&driver.blk_lock);
}
