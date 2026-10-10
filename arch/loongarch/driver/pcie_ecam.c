
#define LOG_MODULE "PCIE_ECAM"

#include "kernel/log.h"
#include "kernel/types.h"
#include "platform/pice_ecam.h"

struct pci_device pci_devices[PCI_MAX_DEVICES];
uint pci_device_count;

void pcie_enumerate()
{
	pci_device_count = 0;

	for (unsigned dev = 0; dev < PCI_MAX_DEVICES; dev++) {
		uint64 addr = ((uint64) dev << PCIE_ECAH_DEV_SHIFT);

		uint16 vendor = pcie_read16(addr + VENDOR_OFFSET);
		if (vendor == INVALID_ID)
			continue;

		uint16 device = pcie_read16(addr + DEVICE_OFFSET);
		uint8 header_type = pcie_read8(addr + HEADER_TYPE_OFFSET);
		uint32 class_code = pcie_read32(addr + REVISION_ID_OFFSET) >> 8;

		struct pci_device *pci_device = &pci_devices[pci_device_count];

		pci_device->bus = 0;
		pci_device->dev = dev;
		pci_device->func = 0;
		pci_device->header_type = header_type;
		pci_device->class_code = class_code;

		pci_device->vendor = vendor;
		pci_device->device = device;

		pci_device_count++;
		LOG_DEBUG("PCI bus=0 dev=0x%x fn=0 vendor=0x%x device=0x%x", dev,
			  (unsigned) vendor, (unsigned) device);
	}

	LOG_DEBUG("bus 0: %d devices", pci_device_count);
}

/*
 * pcie_find_device - find a device by vendor and device id in pci_devices
 * */
struct pci_device *pcie_find_device(uint64 vendor, uint64 device)
{
	for (int i = 0; i < pci_device_count; i++) {
		struct pci_device *pci_device = &pci_devices[i];
		if (pci_device->vendor == vendor &&
		    pci_device->device == device) {
			return pci_device;
		}
	}
	return 0;
}
