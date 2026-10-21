
#define LOG_MODULE "VIRTIO_PCI"

#include "kernel/arch/timer.h"
#include "driver/virtio_pci.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "driver/pci.h"
#include "driver/pci_resource.h"
#include "driver/virtio_transport.h"
#include "driver/virtio_blk.h"
#include "kernel/irq.h"

/* Bound the reset wait independently of interrupt delivery. */
#define BLK_TIMEOUT_TICKS (5ULL * TIMER_FREQ)

static int expired(uint64 start)
{
	return arch_read_time() - start >= BLK_TIMEOUT_TICKS;
}

const char *virtio_pci_cap_type_name(int cfg_type)
{
	switch (cfg_type) {
	case VIRTIO_PCI_CAP_FIRST_PTR:
		return "First capability pointer (driver-defined)";
	case VIRTIO_PCI_CAP_COMMON_CFG:
		return "Common configuration";
	case VIRTIO_PCI_CAP_NOTIFY_CFG:
		return "Notifications";
	case VIRTIO_PCI_CAP_ISR_CFG:
		return "ISR status";
	case VIRTIO_PCI_CAP_DEVICE_CFG:
		return "Device-specific configuration";
	case VIRTIO_PCI_CAP_PCI_CFG:
		return "PCI configuration access";
	case VIRTIO_PCI_CAP_SHARED_MEMORY_CFG:
		return "Shared memory region";
	case VIRTIO_PCI_CAP_VENDOR_CFG:
		return "Vendor-specific data";
	case VIRTIO_PCI_CAP_NONE:
		return "No capability (driver-defined)";
	default:
		return "unknown";
	}
}

static int read_virtio_pci_cap(struct virtio_pci_cap *pci_cap, uint64 addr)
{
	pci_cap->cap_vndr = pcie_read8(addr);
	pci_cap->cap_next = pcie_read8(addr + 1);
	pci_cap->cap_len = pcie_read8(addr + 2);
	pci_cap->cfg_type = pcie_read8(addr + 3);
	pci_cap->bar = pcie_read8(addr + 4);
	pci_cap->id = pcie_read8(addr + 5);
	pci_cap->padding[0] = pcie_read8(addr + 6);
	pci_cap->padding[1] = pcie_read8(addr + 7);
	pci_cap->offset = pcie_read32(addr + 8);
	pci_cap->length = pcie_read32(addr + 12);

	return 0;
}

int virtio_pci_scan_caps(uint64 cfg_addr, const struct pci_device *device,
			 struct virtio_pci_regions *regions)
{
	struct virtio_pci_regions found = {0};
	uint8 visited[PCI_CONFIG_SPACE_SIZE] = {0};

	*regions = (struct virtio_pci_regions){0};
	if (!(pcie_read16(cfg_addr + STATUS_OFFSET) & PCI_STATUS_CAP_LIST)) {
		LOG_ERROR("PCI device has no capability list");
		return -1;
	}

	uint8 pos = pcie_read8(cfg_addr + CAPABILITIES_OFFSET);
	while (pos != 0) {
		if (pos < 0x40 || (pos & 3U) || visited[pos]) {
			LOG_ERROR(
			    "Invalid or repeated PCI capability offset 0x%x",
			    pos);
			return -1;
		}
		visited[pos] = 1;
		uint64 addr = cfg_addr + pos;
		uint8 id = pcie_read8(addr);
		uint8 next = pcie_read8(addr + 1);

		if (id == PCI_CAP_ID_VENDOR) {
			uint8 len = pcie_read8(addr + 2);
			if (len < 3 ||
			    (uint32) pos + len > PCI_CONFIG_SPACE_SIZE) {
				LOG_ERROR(
				    "Invalid vendor capability length at 0x%x",
				    pos);
				return -1;
			}
			/* Short vendor capabilities cannot describe VirtIO
			 * regions. */
			if (len < 16) {
				pos = next;
				continue;
			}
			struct virtio_pci_cap cap;
			read_virtio_pci_cap(&cap, addr);
			struct virtio_pci_cap *target = 0;
			uint32 min_length = 0;
			uint32 alignment = 1;
			switch (cap.cfg_type) {
			case VIRTIO_PCI_CAP_COMMON_CFG:
				target = &found.common;
				min_length = 56; /* Base layout, before optional
						    extensions. */
				alignment = 4;
				break;
			case VIRTIO_PCI_CAP_NOTIFY_CFG:
				target = &found.notify;
				min_length = 2;
				alignment = 2;
				if (len < 20) {
					LOG_ERROR(
					    "Short VirtIO notify capability");
					return -1;
				}
				break;
			case VIRTIO_PCI_CAP_ISR_CFG:
				target = &found.isr;
				min_length = 1;
				break;
			case VIRTIO_PCI_CAP_DEVICE_CFG:
				target = &found.device;
				min_length = 8; /* virtio-blk capacity */
				alignment = 4;
				break;
			default:
				break;
			}

			if (target) {
				if (cap.bar >= 6 ||
				    !device->bars[cap.bar].valid) {
					LOG_ERROR(
					    "Invalid VirtIO capability BAR %d",
					    cap.bar);
					return -1;
				}
				const struct pci_bar *bar =
				    &device->bars[cap.bar];
				if (cap.length < min_length ||
				    cap.offset % alignment ||
				    (uint64) cap.offset > bar->size ||
				    (uint64) cap.length >
					bar->size - cap.offset) {
					LOG_ERROR("Invalid VirtIO region type "
						  "%d in BAR%d",
						  cap.cfg_type, cap.bar);
					return -1;
				}
				/* Preserve the first supported instance in list
				 * order. */
				if (!target->cap_len) {
					*target = cap;
					if (cap.cfg_type ==
					    VIRTIO_PCI_CAP_NOTIFY_CFG)
						found.notify_off_multiplier =
						    pcie_read32(addr + 16);
				}
			}
		}
		pos = next;
	}

	if (!found.common.cap_len || !found.notify.cap_len ||
	    !found.isr.cap_len || !found.device.cap_len) {
		LOG_ERROR("Missing required VirtIO block PCI capabilities");
		return -1;
	}
	found.valid = 1;
	*regions = found;
	return 0;
}

static uint64 pci_cfg_addr(const struct pci_device *device)
{
	return ((uint64) device->bus << PCIE_ECAM_BUS_SHIFT) |
	       ((uint64) device->dev << PCIE_ECAH_DEV_SHIFT) |
	       ((uint64) device->func << PCIE_ECAM_FUNC_SHIFT);
}

static struct pci_device *capabilities_pointer_scan(void)
{
	pcie_enumerate();
	virtio_blk_pci_regions = (struct virtio_pci_regions){0};
	struct pci_device *pci_device = 0;

	pci_device = pci_find_device(PCI_VENDOR_ID, PCI_MODERN_BLK_ID);
	if (!pci_device) {
		LOG_DEBUG(
		    "Modern virtio-blk not found; trying transitional device");
		pci_device = pci_find_device(PCI_VENDOR_ID, PCI_TRANS_BLK_ID);
		if (!pci_device) {
			LOG_DEBUG("virtio blk transition not found");
			return 0;
		}
		LOG_DEBUG(
		    "Transitional virtio-blk found; using modern capabilities");
	}

	uint16 saved_command = 0;
	uint64 cfg_addr = pci_cfg_addr(pci_device);
	if (pci_disable_decoding(cfg_addr, &saved_command) < 0)
		return 0;

	int result = pci_probe_bar(cfg_addr, pci_device);
	/* Restore Command even if BAR probing failed. */
	pci_restore_command(saved_command, cfg_addr);
	if (pcie_read16(cfg_addr + COMMAND_OFFSET) != saved_command) {
		LOG_ERROR("Failed to restore PCI Command");
		return 0;
	}
	if (result < 0)
		return 0;

	if (virtio_pci_scan_caps(cfg_addr, pci_device,
				 &virtio_blk_pci_regions) < 0)
		return 0;
	LOG_DEBUG("VirtIO block PCI configuration regions discovered");

	if (pci_assign_memory_bars(pci_device) < 0) {
		LOG_ERROR("Failed to prepare VirtIO PCI MMIO");
		return 0;
	}
	return pci_device;
}

#ifdef VIRTIO_PCI_TRANSPORT
static volatile struct virtio_pci_common_cfg *common;
static int irq_registered;

static int virtio_pci_probe_device(struct virtio_device *dev, uint32 device_id)
{
	if (device_id != VIRTIO_BLK_ID)
		return -1;
	struct pci_device *pci_device = capabilities_pointer_scan();
	if (pci_device == 0) {
		LOG_ERROR("Failed to find virtio block device");
		return -1;
	}
	// Use `volatile` to prevent the compiler from omitting
	// reads—ensuring that the value is re-read every time,
	// rather than reusing a previously written value.
	common = (volatile struct virtio_pci_common_cfg *)
		     virtio_blk_pci_regions.common_va;
	if (common == 0) {
		LOG_ERROR("Failed to find virtio block common region");
		return -1;
	}

	dev->pci_dev = pci_device;
	dev->modern = 1;
	return 0;
}

static uint8 virtio_pci_get_status(struct virtio_device *dev)
{
	(void) dev;
	return common->device_status;
}

static void virtio_pci_set_status(struct virtio_device *dev, uint8 status)
{
	uint64 cfg = pci_cfg_addr(dev->pci_dev);
	uint16 command = pcie_read16(cfg + COMMAND_OFFSET);
	if (status & VIRTIO_CONFIG_S_FAILED) {
		command =
		    (command | PCI_COMMAND_INTX_DISABLE) & ~PCI_COMMAND_MASTER;
		pcie_write16(cfg + COMMAND_OFFSET, command);
		common->device_status = status;
		return;
	}
	if ((status & VIRTIO_CONFIG_S_DRIVER_OK) &&
	    !(common->device_status & VIRTIO_CONFIG_S_DRIVER_OK)) {
		if (!irq_registered)
			panic("virtio-pci: IRQ handler is not registered");
		command |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
		pcie_write16(cfg + COMMAND_OFFSET, command);
		if (pcie_read16(cfg + COMMAND_OFFSET) != command)
			panic("virtio-pci: cannot enable DMA");
		common->device_status = status;
		if (arch_pci_intx_enable(dev->pci_dev) < 0)
			panic("virtio-pci: cannot enable platform IRQ");
		command &= ~PCI_COMMAND_INTX_DISABLE;
		pcie_write16(cfg + COMMAND_OFFSET, command);
		if (pcie_read16(cfg + COMMAND_OFFSET) != command)
			panic("virtio-pci: cannot unmask INTx");
		return;
	}
	common->device_status = status;
}

static int virtio_pci_reset(struct virtio_device *dev)
{
	uint64 cfg = pci_cfg_addr(dev->pci_dev);
	uint16 command = pcie_read16(cfg + COMMAND_OFFSET);
	command = (command | PCI_COMMAND_MEMORY | PCI_COMMAND_INTX_DISABLE) &
		  ~PCI_COMMAND_MASTER;
	pcie_write16(cfg + COMMAND_OFFSET, command);
	if (pcie_read16(cfg + COMMAND_OFFSET) != command)
		return -1;
	/* This backend uses INTx, never a firmware-programmed MSI-X table. */
	uint8 pos = pcie_read8(cfg + CAPABILITIES_OFFSET);
	uint8 visited[PCI_CONFIG_SPACE_SIZE] = {0};
	while (pos) {
		if (pos < 0x40 || (pos & 3U) || visited[pos])
			goto failed;
		visited[pos] = 1;
		if (pcie_read8(cfg + pos) == 0x11 &&
		    (pcie_read16(cfg + pos + 2) & (1U << 15))) {
			LOG_ERROR("INTx backend requires MSI-X disabled");
			goto failed;
		}
		pos = pcie_read8(cfg + pos + 1);
	}
	/* Always issue reset, even if the old status already reads zero. */
	common->device_status = 0;
	uint64 start = arch_read_time();
	while (common->device_status != 0) {
		if (expired(start)) {
			LOG_ERROR("PCI device reset timed out");
			goto failed;
		}
	}
	return 0;

failed:
	common->device_status |= VIRTIO_CONFIG_S_FAILED;
	return -1;
}

static int virtio_pci_register_irq(struct virtio_device *dev,
				   irq_handler_t handler, void *arg)
{
	if (irq_registered ||
	    arch_pci_intx_register(dev->pci_dev, handler, arg) < 0)
		return -1;
	irq_registered = 1;
	return 0;
}

static uint64 virtio_pci_get_features(struct virtio_device *dev)
{
	common->device_feature_select = 0;
	uint64 features = common->device_feature;
	if (dev->modern) {
		common->device_feature_select = 1;
		features |= (uint64) common->device_feature << 32;
	}
	return features;
}

static void virtio_pci_set_features(struct virtio_device *dev, uint64 features)
{
	common->driver_feature_select = 0;
	common->driver_feature = (uint32) features;

	if (dev->modern) {
		common->driver_feature_select = 1;
		common->driver_feature = (uint32) (features >> 32);
	}
}

static uint8 virtio_pci_queue_ready(struct virtio_device *dev, uint16 queue)
{
	(void) dev;
	/* Select the queue before reading its enable state. */
	common->queue_select = queue;
	return common->queue_enable != 0;
}

static uint32 virtio_pci_get_queue_max_size(struct virtio_device *dev,
					    uint16 queue)
{
	(void) dev;
	common->queue_select = queue;
	return common->queue_size;
}

/* PCI queue addresses are 64-bit fields accessed as two 32-bit halves. */
static void virtio_pci_write_queue_addr(volatile uint64 *reg, uint64 pa)
{
	volatile uint32 *parts = (volatile uint32 *) reg;

	parts[0] = (uint32) pa;
	parts[1] = (uint32) (pa >> 32);
}

static int virtio_pci_setup_queue(struct virtio_device *dev, uint16 queue,
				  uint16 size, uint64 desc_pa, uint64 avail_pa,
				  uint64 used_pa)
{
	/* This backend implements the modern PCI interface. */
	if (!dev->modern || !common)
		return -1;

	/* Split queues require a power-of-two size, at most 32768. */
	if (!size || (size & (size - 1)) || size > 32768)
		return -1;

	/* Descriptor, available and used areas have different alignments. */
	if ((desc_pa & 15U) || (avail_pa & 1U) || (used_pa & 3U))
		return -1;

	if (queue >= common->num_queues)
		return -1;

	common->queue_select = queue;

	/* Do not reconfigure an enabled or unavailable queue. */
	if (common->queue_enable || !common->queue_size ||
	    size > common->queue_size)
		return -1;

	common->queue_size = size;

	/* No MSI-X vector: completion uses PCI INTx. */
	common->queue_msix_vector = 0xffffU;

	/* Addresses are already physical; do not convert them again. */
	virtio_pci_write_queue_addr(&common->queue_desc, desc_pa);
	virtio_pci_write_queue_addr(&common->queue_driver, avail_pa);
	virtio_pci_write_queue_addr(&common->queue_device, used_pa);

	/* Publish initialized queue memory before enabling the queue. */
	__sync_synchronize();
	common->queue_enable = 1;

	if (common->queue_enable != 1 || common->queue_size != size)
		return -1;

	return 0;
}

static void virtio_pci_notify_queue(struct virtio_device *dev, uint16 queue)
{
	(void) dev;
	const struct virtio_pci_regions *regions = &virtio_blk_pci_regions;

	common->queue_select = queue;

	uint64 offset =
	    (uint64) common->queue_notify_off * regions->notify_off_multiplier;

	/* Validate the queue's notification address before accessing it. */
	if ((offset & 1U) || offset > regions->notify.length ||
	    regions->notify.length - offset < sizeof(uint16))
		panic("virtio_pci_notify_queue: invalid notify offset");

	volatile uint16 *notify =
	    (volatile uint16 *) (regions->notify_va + offset);

	/* Publish descriptors and the available index before notifying. */
	__sync_synchronize();
	*notify = queue;
}

static uint8 virtio_pci_ack_interrupt(struct virtio_device *dev)
{
	(void) dev;
	volatile uint8 *isr = (volatile uint8 *) virtio_blk_pci_regions.isr_va;

	return *isr & 3U;
}

const struct virtio_transport_ops virtio_blk_device_ops = {
    .probe_device = virtio_pci_probe_device,
    .reset = virtio_pci_reset,
    .get_status = virtio_pci_get_status,
    .set_status = virtio_pci_set_status,
    .get_features = virtio_pci_get_features,
    .set_features = virtio_pci_set_features,
    .get_queue_ready = virtio_pci_queue_ready,
    .get_queue_max_size = virtio_pci_get_queue_max_size,
    .setup_queue = virtio_pci_setup_queue,
    .notify_queue = virtio_pci_notify_queue,
    .ack_interrupt = virtio_pci_ack_interrupt,
    .register_irq = virtio_pci_register_irq,
};

struct virtio_device virtio_blk_device = {
    .ops = &virtio_blk_device_ops,
    .pci_dev = 0,
    .transport_data = 0,
    .modern = 0,
};

#endif /* VIRTIO_PCI_TRANSPORT */
