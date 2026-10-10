
#define LOG_MODULE "VIRTIO_PCI"

#include "driver/virtio_pci.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "platform/pice_ecam.h"

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

void capabilities_pointer_scan()
{
	uint64 addr = 0;
	uint64 dev = 0;
	struct pci_device *pci_device = 0;

	pci_device = pcie_find_device(PCI_VENDOR_ID, PCI_MODERN_BLK_ID);
	if (pci_device == 0) {
		LOG_DEBUG("virtio blk modern transition not foun");
		pci_device = pcie_find_device(PCI_VENDOR_ID, PCI_TRANS_BLK_ID);
		if (pci_device == 0) {
			LOG_DEBUG("virtio blk transition not found");
		}
		LOG_DEBUG("virtio blk legacy transition found");
    return;
	}

	dev = pci_device->dev;

	for (int i = 0; i < PCI_MAX_FUNCTIONS; i++) {
		addr =
		    (dev << PCIE_ECAH_DEV_SHIFT) + (i << PCIE_ECAM_FUNC_SHIFT);
		// This register points to the linked list of capability
		// structures implemented by the Function. Since every PCI
		// Express Function must implement a PCI Express Capability
		// structure, and that structure must be included in this list,
		// the value of this register must be non-zero.
		uint8 capabilities_ptr = pcie_read8(addr + CAPABILITIES_OFFSET);
		addr += capabilities_ptr;
		struct virtio_pci_cap pci_cap;
		read_virtio_pci_cap(&pci_cap, addr);
	}
}
