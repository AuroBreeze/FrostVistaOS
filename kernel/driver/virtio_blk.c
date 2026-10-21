
#define LOG_MODULE "VIRT"

#include "kernel/arch/mm.h"
#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "driver/virtio.h"
#include "driver/virtio_blk.h"
#include "driver/virtio_transport.h"

struct VirtioBlkDrvier driver;

static int virtio_blk_irq_handler(void *arg);

static uint64 align_up(uint64 value, uint64 align)
{
	return (value + align - 1) & ~(align - 1);
}

static void *legacy_queue_alloc()
{
	uint8 *page0 = kalloc();
	uint8 *page1 = kalloc();
	if (!page0 || !page1) {
		return 0;
	}

	if (page0 + PGSIZE == page1) {
		return page0;
	}
	if (page1 + PGSIZE == page0) {
		return page1;
	}

	panic("virtio_blk legacy queue pages are not contiguous");
	__builtin_unreachable();
}

void init_free_desc_list()
{
	for (int i = 0; i < NUM; i++) {
		driver.vq.desc[i].next = i + 1;
	}

	driver.vq.desc[NUM - 1].next = 0xff;
	driver.free_desc_idx = 0;
}

// alloc_desc - Allocate a free descriptor
int alloc_desc()
{
	if (driver.free_desc_idx == 0xff) {
		return -1;
	}

	int idx = driver.free_desc_idx;
	driver.free_desc_idx = driver.vq.desc[idx].next;
	return idx;
}

int free_desc(int idx)
{
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

void insert_free_desc_list(int idx)
{
	if (free_desc(idx) < 0) {
		return;
	}
	int current = driver.free_desc_idx;
	driver.free_desc_idx = idx;
	driver.vq.desc[idx].next = current;
}

void free_chain(int idx)
{
	if (idx < 0 || idx >= NUM) {
		LOG_ERROR("free_desc idx out of range");
		return;
	}
	while (idx != 0xff) {
		int next = driver.vq.desc[idx].next;
		insert_free_desc_list(idx);
		idx = next;
	}
	wakeup(&driver.free_desc_idx);
}

// xv6
// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
static int alloc3_desc(int *idx)
{
	for (int i = 0; i < 3; i++) {
		idx[i] = alloc_desc();
		if (idx[i] < 0) {
			for (int j = 0; j < i; j++)
				insert_free_desc_list(idx[j]);
			return -1;
		}
	}
	return 0;
}

// virtio_disk_rw - Read/Write block
void virtio_disk_rw(struct buf *buffer, int write)
{

	acquire(&driver.blk_lock);

	if (!driver.vq.desc || !driver.vq.avail || !driver.vq.used) {
		panic("virtio_disk_rw: virtqueue is not initialized");
	}

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
	driver.vq.desc[idx[0]].addr = arch_kva_to_pa((uint64) req);
	driver.vq.desc[idx[0]].len = sizeof(struct virtio_blk_req);
	driver.vq.desc[idx[0]].flags = VIRTQ_DESC_F_NEXT;
	driver.vq.desc[idx[0]].next = idx[1];

	// The buffer to be written to or read from
	driver.vq.desc[idx[1]].addr = arch_kva_to_pa((uint64) buffer->data);
	driver.vq.desc[idx[1]].len = BSIZE;
	if (write) {
		driver.vq.desc[idx[1]].flags =
		    0; // Data flows from `buf->data` to the disk
	} else {
		driver.vq.desc[idx[1]].flags =
		    VIRTQ_DESC_F_WRITE; // The disk loads the data into
					// `buf->data`
	}
	driver.vq.desc[idx[1]].flags |= VIRTQ_DESC_F_NEXT;
	driver.vq.desc[idx[1]].next = idx[2];

	// virtio_blk_req.status
	// Split into separate arrays
	driver.vq.desc[idx[2]].addr =
	    arch_kva_to_pa((uint64) &driver.info[idx[0]].status);
	driver.vq.desc[idx[2]].len = 1;
	driver.vq.desc[idx[2]].flags = VIRTQ_DESC_F_WRITE;
	driver.vq.desc[idx[2]].next = 0xff;

	buffer->done = 0;
	driver.info[idx[0]].buffer = buffer;

	// tell the device the first index in our chain of descriptors.
	driver.vq.avail->ring[driver.vq.avail->idx % NUM] = idx[0];

	__sync_synchronize();

	driver.vq.avail->idx += 1;

	__sync_synchronize();

	const struct virtio_transport_ops *ops = virtio_blk_device.ops;
	// Writing a value to this register notifies the device that there are
	// new buffers to process in a queue.
	ops->notify_queue(&virtio_blk_device, 0);

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
void virtio_disk_init()
{
	// The transport handles registers; this layer controls initialization
	// order and selects the block device features to negotiate.
	struct virtio_device *dev = &virtio_blk_device;
	const struct virtio_transport_ops *ops = dev->ops;
	initlock(&driver.blk_lock, "virtio_blk");

	// Probe checks the device type and sets modern. Reset waits for the
	//  device status to become zero before initialization continues.
	if (ops->probe_device(dev, VIRTIO_BLK_ID) < 0)
		panic("virtio_disk_init: device probe failed");
	if (ops->reset(dev) < 0)
		panic("virtio_disk_init: reset failed");

	// Accumulate status bits: acknowledge discovery, then claim the device.
	uint8 status = VIRTIO_CONFIG_S_ACKNOWLEDGE;
	ops->set_status(dev, status);
	status |= VIRTIO_CONFIG_S_DRIVER;
	ops->set_status(dev, status);

	uint64 offered = ops->get_features(dev);
	uint64 features = 0;
	// The request path does not enforce read-only access. Declining RO does
	// not make a read-only disk writable, so reject it here.

	if (offered & (1ULL << VIRTIO_BLK_F_RO)) {
		ops->set_status(dev, status | VIRTIO_CONFIG_S_FAILED);
		panic("virtio_disk_init: writable block driver cannot use "
		      "read-only disk");
	}
	/* Accept only implemented features, leaving packed rings and EVENT_IDX
	 * disabled. Modern requires VERSION_1 (bit 32); legacy accepts no
	 * optional features here.
	 */
	if (dev->modern) {
		if (!(offered & (1ULL << 32))) {
			ops->set_status(dev, status | VIRTIO_CONFIG_S_FAILED);
			panic(
			    "virtio_disk_init: VIRTIO_F_VERSION_1 is missing");
		}
		features = 1ULL << 32;
	}
	ops->set_features(dev, features);
	if (dev->modern) {
		// Modern requires FEATURES_OK readback; legacy skips this step.
		status |= VIRTIO_CONFIG_S_FEATURES_OK;
		ops->set_status(dev, status);
		status = ops->get_status(dev);
		if (!(status & VIRTIO_CONFIG_S_FEATURES_OK) ||
		    (status & VIRTIO_CONFIG_S_DEVICE_NEEDS_RESET)) {
			ops->set_status(dev, status | VIRTIO_CONFIG_S_FAILED);
			panic("virtio_disk_init: feature negotiation failed");
		}
	}

	/* Block requests use queue 0. Its size and addresses cannot be changed
	 * directly while the queue is enabled.
	 */
	uint32 queue_num_max = ops->get_queue_max_size(dev, 0);
	if (ops->get_queue_ready(dev, 0) || queue_num_max < VIRTIO_BLK_Q_SIZE) {
		ops->set_status(dev, status | VIRTIO_CONFIG_S_FAILED);
		panic("virtio_disk_init: queue 0 is busy or too small (%d)",
		      queue_num_max);
	}
	LOG_TRACE("QueueNumMax: %d", queue_num_max);

	if (!dev->modern) {
		/* Legacy exposes only a starting PFN, so all three queue
		 * regions must share one contiguous physical allocation.
		 */
		uint8 *queue = legacy_queue_alloc();
		if (!queue) {
			ops->set_status(dev, status | VIRTIO_CONFIG_S_FAILED);
			panic(
			    "virtio_disk_init: legacy queue allocation failed");
		}
		driver.vq.desc = (struct virtq_desc *) queue;
		driver.vq.avail =
		    (struct virtq_avail *) (queue + (sizeof(struct virtq_desc) *
						     VIRTIO_BLK_Q_SIZE));
		/* Align the used ring to GuestPageSize, matching QueueAlign in
		 * the transport backend.
		 */
		uint64 used_addr = align_up((uint64) driver.vq.avail +
						sizeof(struct virtq_avail),
					    PGSIZE);
		driver.vq.used = (struct virtq_used *) used_addr;
		if (used_addr + sizeof(struct virtq_used) >
		    (uint64) queue + ((uint64) PGSIZE * 2)) {
			ops->set_status(dev, status | VIRTIO_CONFIG_S_FAILED);
			panic(
			    "virtio_disk_init: legacy queue exceeds two pages");
		}
	} else {
		/* Modern configures each region separately, allowing
		 * independent zero-initialized pages.
		 */
		driver.vq.avail = kalloc();
		driver.vq.used = kalloc();
		driver.vq.desc = kalloc();
	}
	if (!driver.vq.avail || !driver.vq.used || !driver.vq.desc) {
		ops->set_status(dev, status | VIRTIO_CONFIG_S_FAILED);
		panic("virtio_disk_init: queue allocation failed");
	}

	// Prepare the descriptor free list and completion index before the
	// transport enables the queue.
	init_free_desc_list();
	driver.last_used_idx = 0;
	/* Convert kalloc's kernel virtual addresses exactly once. The backend
	 * receives physical addresses and handles the legacy PFN or modern
	 * queue addresses, then enables the queue.
	 */
	if (ops->setup_queue(dev, 0, VIRTIO_BLK_Q_SIZE,
			     arch_kva_to_pa((uint64) driver.vq.desc),
			     arch_kva_to_pa((uint64) driver.vq.avail),
			     arch_kva_to_pa((uint64) driver.vq.used)) < 0) {
		ops->set_status(dev, status | VIRTIO_CONFIG_S_FAILED);
		panic("virtio_disk_init: queue setup failed");
	}

	/* Register while INTx is masked; DRIVER_OK lets the PCI backend unmask.
	 */
	if (ops->register_irq &&
	    ops->register_irq(dev, virtio_blk_irq_handler, dev) < 0) {
		ops->set_status(dev, status | VIRTIO_CONFIG_S_FAILED);
		panic("virtio_disk_init: IRQ registration failed");
	}

	// Set DRIVER_OK after queue setup so the device can process requests.
	status |= VIRTIO_CONFIG_S_DRIVER_OK;
	ops->set_status(dev, status);
	status = ops->get_status(dev);
	if (!(status & VIRTIO_CONFIG_S_DRIVER_OK) ||
	    (status & VIRTIO_CONFIG_S_DEVICE_NEEDS_RESET)) {
		ops->set_status(dev, status | VIRTIO_CONFIG_S_FAILED);
		panic("virtio_disk_init: device did not become ready");
	}
	LOG_INFO("virtio-blk initialized, interface version %d",
		 dev->modern ? 2 : 1);
}

static int virtio_blk_irq_handler(void *arg)
{
	struct virtio_device *dev = arg;
	acquire(&driver.blk_lock);

	const struct virtio_transport_ops *ops = dev->ops;
	uint8 reason = ops->ack_interrupt(dev);
	if (!reason) {
		release(&driver.blk_lock);
		return 0;
	}
	__sync_synchronize();

	// The Golden Rule of Ring Buffers:
	// Compare OUR software bookmark (last_used_idx) with the HARDWARE's
	// index (used->idx)
	while (driver.last_used_idx != driver.vq.used->idx) {

		__sync_synchronize();

		// Read the ID of the completed descriptor from the used ring
		int id = driver.vq.used->ring[driver.last_used_idx % NUM].id;

		if (id < 0 || id >= NUM || !driver.info[id].buffer)
			panic("virtio_disk_intr: invalid used descriptor");

		// Check the status byte written by the device
		if (driver.info[id].status != 0) {
			panic("virtio_disk_intr status error");
		}

		// Retrieve the buffer waiting for this I/O
		struct buf *buffer = driver.info[id].buffer;
		buffer->done = 1;

		// Wake up the specific process sleeping on this buffer's 'done'
		// flag
		wakeup(buffer);

		driver.last_used_idx++;
	}

	release(&driver.blk_lock);
	return 1;
}

void virtio_disk_intr(void)
{
	virtio_blk_irq_handler(&virtio_blk_device);
}
