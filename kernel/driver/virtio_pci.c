
#define LOG_MODULE "VIRTIO_PCI"

#include "driver/virtio_pci.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "driver/pci.h"
#include "driver/pci_resource.h"

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

int read_virtio_pci_cap(struct virtio_pci_cap *pci_cap, uint64 addr)
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

struct pci_device *capabilities_pointer_scan(void)
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
		}
		if (!pci_device) {
			return 0;
		}
		LOG_DEBUG(
		    "Transitional virtio-blk found; using modern capabilities");
	}

	uint16 saved_command = 0;
	uint64 cfg_addr = ((uint64) pci_device->bus << PCIE_ECAM_BUS_SHIFT) |
			  ((uint64) pci_device->dev << PCIE_ECAH_DEV_SHIFT) |
			  ((uint64) pci_device->func << PCIE_ECAM_FUNC_SHIFT);

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
