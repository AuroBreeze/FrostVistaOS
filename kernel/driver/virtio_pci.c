
#define LOG_MODULE "VIRTIO_PCI"

#include "asm/mm.h"
#include "asm/loongarch.h"

#include "driver/virtio_pci.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "platform/pice_ecam.h"

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

int command_close_io_and_memory(uint64 cfg_addr, uint16 *saved_command)
{
	*saved_command = pcie_read16(cfg_addr + COMMAND_OFFSET);
	// close IO and memory
	uint16 probe_command =
	    *saved_command & ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
	pcie_write16(cfg_addr + COMMAND_OFFSET, probe_command);

	// test write command register
	uint16 command = pcie_read16(cfg_addr + COMMAND_OFFSET);
	if (command & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY)) {
		pcie_write16(cfg_addr + COMMAND_OFFSET, *saved_command);
		LOG_ERROR("Failed to disable PCI BAR decoding");
		return -1;
	}
	return 0;
}

void restore_command(uint16 saved_command, uint64 cfg_addr)
{
	// restore command register
	pcie_write16(cfg_addr + COMMAND_OFFSET, saved_command);
}

int bar_scan(uint64 cfg_addr, struct pci_device *device)
{
	for (int i = 0; i < 6; i++)
		device->bars[i] = (struct pci_bar) {0};

	if ((device->header_type & 0x7f) != 0) {
		LOG_ERROR("BAR scan requires a Type 0 header");
		return -1;
	}

	for (int i = 0; i < 6; i++) {
		uint64 reg = cfg_addr + BAR_OFFSET + (uint32) (i * 4);
		uint32 old_low = pcie_read32(reg);
		uint32 type = (old_low >> 1) & 3U;
		struct pci_bar entry = {0};

		entry.is_io = old_low & 1U;
		if (!entry.is_io && type != 0 && type != 2) {
			LOG_ERROR("Unsupported BAR%d memory type %d", i, type);
			goto failed;
		}
		entry.is_64 = !entry.is_io && type == 2;
		entry.prefetchable = !entry.is_io && !!(old_low & 8U);

		if (entry.is_64) {
			if (i == 5) {
				LOG_ERROR("Invalid 64-bit BAR at slot 5");
				goto failed;
			}
			uint32 old_high = pcie_read32(reg + 4);
			pcie_write32(reg, 0xffffffffU);
			pcie_write32(reg + 4, 0xffffffffU);
			uint32 probe_low = pcie_read32(reg);
			uint32 probe_high = pcie_read32(reg + 4);

			/* Restore both DWORDs before checking or returning. */
			pcie_write32(reg, old_low);
			pcie_write32(reg + 4, old_high);
			if (pcie_read32(reg) != old_low ||
			    pcie_read32(reg + 4) != old_high) {
				LOG_ERROR("BAR%d restore mismatch", i);
				goto failed;
			}

			uint64 mask =
			    ((uint64) probe_high << 32) | (probe_low & ~0xfU);
			if (mask != 0) {
				entry.base = ((uint64) old_high << 32) |
					     (old_low & ~0xfU);
				entry.size = ~mask + 1ULL;
				entry.valid = 1;
			}
		} else {
			pcie_write32(reg, 0xffffffffU);
			uint32 probe = pcie_read32(reg);
			pcie_write32(reg, old_low);
			if (pcie_read32(reg) != old_low) {
				LOG_ERROR("BAR%d restore mismatch", i);
				goto failed;
			}

			uint32 address_mask = entry.is_io ? ~0x3U : ~0xfU;
			uint32 mask = probe & address_mask;
			if (mask != 0) {
				entry.base = old_low & address_mask;
				entry.size = (uint32) (~mask + 1U);
				entry.valid = 1;
			}
		}

		if (entry.valid) {
			if (!entry.size || (entry.size & (entry.size - 1)) ||
			    (entry.base & (entry.size - 1))) {
				LOG_ERROR("Invalid BAR%d size or alignment", i);
				goto failed;
			}
			device->bars[i] = entry;
			LOG_DEBUG(
			    "BAR%d: %s %d-bit base=%p size=%p prefetch=%d", i,
			    entry.is_io ? "IO" : "MEM", entry.is_64 ? 64 : 32,
			    entry.base, entry.size, entry.prefetchable);
		}
		if (entry.is_64)
			i++;
	}
	return 0;

failed:
	/* Do not expose partial results after a failed scan. */
	for (int i = 0; i < 6; i++)
		device->bars[i] = (struct pci_bar) {0};
	return -1;
}

struct virtio_pci_regions virtio_blk_pci_regions;

int virtio_pci_scan_caps(uint64 cfg_addr, const struct pci_device *device,
			 struct virtio_pci_regions *regions)
{
	struct virtio_pci_regions found = {0};
	uint8 visited[PCI_CONFIG_SPACE_SIZE] = {0};

	*regions = (struct virtio_pci_regions) {0};
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

/* Early boot only: enumerate/probe resources before assigning any new BARs. */
static int collect_pci_resources(struct pci_device *target)
{
	for (uint n = 0; n < pci_device_count; n++) {
		struct pci_device *device = &pci_devices[n];
		if (device == target)
			continue;
		/* Bridge windows need a separate resource allocator. */
		if ((device->header_type & 0x7f) != 0) {
			LOG_ERROR(
			    "PCI bridge/resource layout is not supported");
			return -1;
		}
		uint64 cfg = pci_cfg_addr(device);
		uint16 saved;
		if (command_close_io_and_memory(cfg, &saved) < 0)
			return -1;
		int result = bar_scan(cfg, device);
		restore_command(saved, cfg);
		if (pcie_read16(cfg + COMMAND_OFFSET) != saved || result < 0) {
			LOG_ERROR("Failed to collect PCI resources");
			return -1;
		}
	}
	return 0;
}

/* Return an overlapping resource's end, or zero when the range is free. */
static uint64 pci_memory_conflict(uint64 base, uint64 size,
				  const struct pci_bar *self)
{
	for (uint n = 0; n < pci_device_count; n++) {
		for (int i = 0; i < 6; i++) {
			const struct pci_bar *bar = &pci_devices[n].bars[i];
			if (bar == self || !bar->valid || bar->is_io ||
			    !bar->base)
				continue;

			// Reject malformed firmware assignments conservatively.
			if (bar->base + bar->size < bar->base)
				return ~0ULL;
			if (base < bar->base + bar->size &&
			    bar->base < base + size)
				return bar->base + bar->size;
		}
	}
	return 0;
}

int mmap_bar(struct pci_device *device)
{
	struct virtio_pci_regions *regions = &virtio_blk_pci_regions;
	const struct virtio_pci_cap *caps[4] = {&regions->common,
						&regions->notify, &regions->isr,
						&regions->device};
	struct {
		uint64 start, end, mapped_end;
	} ranges[4] = {0};
	uint64 old_base[6];
	uint32 old_raw[6];
	uint64 cfg = pci_cfg_addr(device);
	uint64 window_end = PCIE_ECAM_MMIO_PA + PCIE_ECAM_MMIO_RANGE;
	uint16 saved;
	int count = 0;

	if (!regions->valid)
		return -1;
	if (regions->mapped)
		return 0;
	/* This path implements MMIO, not PCI I/O-port accesses. */
	for (int i = 0; i < 4; i++) {
		if (caps[i]->bar >= 6 || !device->bars[caps[i]->bar].valid ||
		    device->bars[caps[i]->bar].is_io) {
			LOG_ERROR("VirtIO configuration requires a Memory BAR");
			return -1;
		}
	}
	if (collect_pci_resources(device) < 0 ||
	    command_close_io_and_memory(cfg, &saved) < 0)
		return -1;
	for (int i = 0; i < 6; i++) {
		old_base[i] = device->bars[i].base;
		old_raw[i] = pcie_read32(cfg + BAR_OFFSET + (uint32) (i * 4));
	}

	/* Validate preassigned resources before allocating the missing ones. */
	for (int i = 0; i < 6; i++) {
		struct pci_bar *bar = &device->bars[i];
		if (!bar->valid || bar->is_io)
			continue;
		if (!bar->size || (bar->size & (bar->size - 1)) ||
		    bar->size > PCIE_ECAM_MMIO_RANGE)
			goto failed;
		if (bar->base &&
		    (bar->base < PCIE_ECAM_MMIO_PA || bar->base >= window_end ||
		     bar->size > window_end - bar->base ||
		     (bar->base & (bar->size - 1)) ||
		     pci_memory_conflict(bar->base, bar->size, bar))) {
			LOG_ERROR("BAR%d has an invalid or overlapping address",
				  i);
			goto failed;
		}
	}
	for (int i = 0; i < 6; i++) {
		struct pci_bar *bar = &device->bars[i];
		if (!bar->valid || bar->is_io || bar->base)
			continue;
		uint64 align = bar->size > PGSIZE ? bar->size : PGSIZE;
		uint64 candidate = PCIE_ECAM_MMIO_PA;
		for (;;) {
			if (candidate > window_end)
				goto failed;
			candidate = (candidate + align - 1) & ~(align - 1);
			if (candidate >= window_end ||
			    bar->size > window_end - candidate) {
				LOG_ERROR("PCI Memory window exhausted");
				goto failed;
			}
			uint64 conflict =
			    pci_memory_conflict(candidate, bar->size, bar);
			if (!conflict)
				break;
			candidate = conflict;
		}
		bar->base = candidate;
		uint64 reg = cfg + BAR_OFFSET + (uint64) (i * 4);
		pcie_write32(reg, (uint32) candidate | (old_raw[i] & 0xfU));
		if (bar->is_64)
			pcie_write32(reg + 4, (uint32) (candidate >> 32));
		uint64 readback = pcie_read32(reg) & ~0xfU;
		if (bar->is_64)
			readback |= (uint64) pcie_read32(reg + 4) << 32;
		if (readback != candidate) {
			LOG_ERROR("BAR%d address write failed", i);
			goto failed;
		}
	}

	/* Map only needed configuration pages, merging shared pages first. */
	for (int i = 0; i < 4; i++) {
		const struct pci_bar *bar = &device->bars[caps[i]->bar];
		if ((uint64) caps[i]->offset > bar->size || !caps[i]->length ||
		    (uint64) caps[i]->length > bar->size - caps[i]->offset)
			goto failed;
		uint64 pa = bar->base + caps[i]->offset;
		ranges[i].start = PGROUNDDOWN(pa);
		ranges[i].end = PGROUNDUP(pa + caps[i]->length);
	}
	for (int i = 0; i < 4; i++)
		for (int j = i + 1; j < 4; j++)
			if (ranges[j].start < ranges[i].start) {
				uint64 start = ranges[i].start;
				uint64 end = ranges[i].end;

				ranges[i].start = ranges[j].start;
				ranges[i].end = ranges[j].end;
				ranges[j].start = start;
				ranges[j].end = end;
			}
	for (int i = 0; i < 4; i++) {
		if (count && ranges[i].start <= ranges[count - 1].end) {
			if (ranges[i].end > ranges[count - 1].end)
				ranges[count - 1].end = ranges[i].end;
		} else {
			ranges[count].start = ranges[i].start;
			ranges[count].end = ranges[i].end;
			count++;
		}
	}
	for (int i = 0; i < count; i++)
		ranges[i].mapped_end = ranges[i].start;
	for (int i = 0; i < count; i++) {
		for (uint64 pa = ranges[i].start; pa < ranges[i].end;
		     pa += PGSIZE) {
			uint64 va = KERNEL_IO_BASE + pa;
			if (kvmmap_mmio_current(va, pa, PGSIZE,
						LA_PTE_PLV0 | LA_PTE_W |
						    LA_PTE_NX |
						    LA_PTE_MAT_SUC) < 0) {
				LOG_ERROR("PCI MMIO mapping failed at %p", va);
				goto failed;
			}
			ranges[i].mapped_end = pa + PGSIZE;
		}
	}
	invtlb_all();
	pcie_write16(cfg + COMMAND_OFFSET, saved | PCI_COMMAND_MEMORY);
	if (pcie_read16(cfg + COMMAND_OFFSET) != (saved | PCI_COMMAND_MEMORY)) {
		LOG_ERROR("Failed to enable PCI Memory decoding");
		goto failed;
	}
	regions->common_va = KERNEL_IO_BASE +
			     device->bars[regions->common.bar].base +
			     regions->common.offset;
	regions->notify_va = KERNEL_IO_BASE +
			     device->bars[regions->notify.bar].base +
			     regions->notify.offset;
	regions->isr_va = KERNEL_IO_BASE + device->bars[regions->isr.bar].base +
			  regions->isr.offset;
	regions->device_va = KERNEL_IO_BASE +
			     device->bars[regions->device.bar].base +
			     regions->device.offset;
	regions->mapped = 1;
	LOG_INFO("VirtIO PCI MMIO ready: common=%p notify=%p isr=%p device=%p",
		 regions->common_va, regions->notify_va, regions->isr_va,
		 regions->device_va);
	return 0;

failed:
	// TODO: Track incomplete rollback as an unusable device state and
	// propagate it to callers, blocking retries/access after BAR or Command
	// restoration fails instead of only logging the failure.

	// Disable decoding before restoring addresses; remove only our pages.
	pcie_write16(cfg + COMMAND_OFFSET,
		     saved & ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY));
	if (pcie_read16(cfg + COMMAND_OFFSET) &
	    (PCI_COMMAND_IO | PCI_COMMAND_MEMORY)) {
		LOG_ERROR("Cannot disable PCI decoding for rollback");
		// TODO: Retain ownership of surviving mappings and BAR
		// allocations for recovery once the device can be quiesced;
		// cleanup is incomplete.
		return -1;
	}
	// TODO: Track page-table pages allocated by this mapping attempt and
	// reclaim those left empty after removing its leaf PTEs, including on
	// partial mapping failure. Preserve pre-existing/shared page tables.
	for (int i = 0; i < count; i++)
		for (uint64 pa = ranges[i].start; pa < ranges[i].mapped_end;
		     pa += PGSIZE) {
			pte_t *pte = walk_current(KERNEL_IO_BASE + pa, 0);
			if (pte)
				*pte = 0;
		}
	invtlb_all();
	for (int i = 0; i < 6; i++) {
		pcie_write32(cfg + BAR_OFFSET + (uint32) (i * 4), old_raw[i]);
		device->bars[i].base = old_base[i];
	}
	for (int i = 0; i < 6; i++)
		if (pcie_read32(cfg + BAR_OFFSET + (uint32) (i * 4)) !=
		    old_raw[i]) {
			LOG_ERROR(
			    "BAR%d rollback failed; decoding remains disabled",
			    i);
			return -1;
		}
	restore_command(saved, cfg);
	if (pcie_read16(cfg + COMMAND_OFFSET) != saved)
		LOG_ERROR("PCI Command rollback failed");
	return -1;
}

void capabilities_pointer_scan()
{
	virtio_blk_pci_regions = (struct virtio_pci_regions) {0};
	struct pci_device *pci_device = 0;

	pci_device = pcie_find_device(PCI_VENDOR_ID, PCI_MODERN_BLK_ID);
	if (!pci_device) {
		LOG_DEBUG("virtio blk modern transition not foun");
		pci_device = pcie_find_device(PCI_VENDOR_ID, PCI_TRANS_BLK_ID);
		if (!pci_device) {
			LOG_DEBUG("virtio blk transition not found");
		}
		LOG_DEBUG("virtio blk legacy transition found");
		if (!pci_device) {
			return;
		}
	}

	uint16 saved_command = 0;
	uint64 cfg_addr = ((uint64) pci_device->bus << PCIE_ECAM_BUS_SHIFT) |
			  ((uint64) pci_device->dev << PCIE_ECAH_DEV_SHIFT) |
			  ((uint64) pci_device->func << PCIE_ECAM_FUNC_SHIFT);

	if (command_close_io_and_memory(cfg_addr, &saved_command) < 0)
		return;

	int result = bar_scan(cfg_addr, pci_device);
	/* Restore Command even if BAR probing failed. */
	restore_command(saved_command, cfg_addr);
	if (pcie_read16(cfg_addr + COMMAND_OFFSET) != saved_command) {
		LOG_ERROR("Failed to restore PCI Command");
		return;
	}
	if (result < 0)
		return;

	if (virtio_pci_scan_caps(cfg_addr, pci_device,
				 &virtio_blk_pci_regions) < 0)
		return;
	LOG_DEBUG("VirtIO block PCI configuration regions discovered");

	if (mmap_bar(pci_device) < 0)
		LOG_ERROR("Failed to prepare VirtIO PCI MMIO");
}
