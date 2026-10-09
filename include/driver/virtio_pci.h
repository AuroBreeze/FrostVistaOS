#ifndef FV_DRIVER_VIRTIO_PCI_H
#define FV_DRIVER_VIRTIO_PCI_H

#include "kernel/types.h"

/*
 * https://docs.oasis-open.org/virtio/virtio/v1.2/cs01/virtio-v1.2-cs01.html#x1-1150001
 *
 * Any PCI device with PCI Vendor ID 0x1AF4, and PCI Device ID 0x1000 through
 * 0x107F inclusive is a virtio device. The actual value within this range
 * indicates which virtio device is supported by the device. The PCI Device ID
 * is calculated by adding 0x1040 to the Virtio Device ID, as indicated in
 * section 5. Additionally, devices MAY utilize a Transitional PCI Device ID
 * range, 0x1000 to 0x103F depending on the device type.
 * */
#define PCI_VENDOR_ID 0x1AF4
#define PCI_DEVICE_ID_BASE 0x1000

// Transitional Pci Device Id
#define PCI_TRANS_BLK_ID 0x1001
// Modern Pci Device Id
#define PCI_MODERN_BLK_ID 0x1042

/* offset and length are stored in little-endian order. */
struct virtio_pci_cap {
	uint8 cap_vndr; /* PCI_CAP_ID_VNDR */
	uint8 cap_next; /* Next capability offset in configuration space. */
	uint8 cap_len;	/* Capability length in bytes. */
	uint8 cfg_type; /* Identifies the VirtIO configuration structure. */
	uint8 bar;	/* BAR containing the structure. */
	uint8 id;	/* Identifies capabilities of the same type. */
	uint8 padding[2];
	uint32 offset; /* Little-endian byte offset within the BAR. */
	uint32 length; /* Little-endian structure length in bytes. */
};

#endif
