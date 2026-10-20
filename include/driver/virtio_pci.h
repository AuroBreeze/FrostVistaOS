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

enum virtio_pci_cap_type {
	/* Driver-defined marker for the first capability pointer. */
	VIRTIO_PCI_CAP_FIRST_PTR = 0,
	VIRTIO_PCI_CAP_COMMON_CFG = 1, /* Common configuration */
	VIRTIO_PCI_CAP_NOTIFY_CFG = 2, /* Notifications */
	VIRTIO_PCI_CAP_ISR_CFG = 3,    /* ISR status */
	VIRTIO_PCI_CAP_DEVICE_CFG = 4, /* Device-specific configuration */
	VIRTIO_PCI_CAP_PCI_CFG = 5,    /* PCI configuration access */
	VIRTIO_PCI_CAP_SHARED_MEMORY_CFG = 8, /* Shared memory region */
	VIRTIO_PCI_CAP_VENDOR_CFG = 9,	      /* Vendor-specific data */
	/* Driver-defined sentinel for an absent capability. */
	VIRTIO_PCI_CAP_NONE = 0xff,
};

// 16B
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

// virtio v1.2 chapter 4.1.4.3 Common configuration structure layout
// The common configuration structure is found at the bar and offset within the
// VIRTIO_PCI_CAP_COMMON_CFG capability; its layout is below.
/* Multi-byte fields are stored in little-endian order. */
struct virtio_pci_common_cfg {
	/* About the whole device. */
	uint32 device_feature_select; /* read-write */
	uint32 device_feature;	      /* read-only for driver */
	uint32 driver_feature_select; /* read-write */
	uint32 driver_feature;	      /* read-write */
	uint16 config_msix_vector;    /* read-write */
	uint16 num_queues;	      /* read-only for driver */
	uint8 device_status;	      /* read-write */
	uint8 config_generation;      /* read-only for driver */

	/* About a specific virtqueue. */
	uint16 queue_select;	  /* read-write */
	uint16 queue_size;	  /* read-write */
	uint16 queue_msix_vector; /* read-write */
	uint16 queue_enable;	  /* read-write */
	uint16 queue_notify_off;  /* read-only for driver */
	uint64 queue_desc;	  /* read-write */
	uint64 queue_driver;	  /* read-write */
	uint64 queue_device;	  /* read-write */
	uint16 queue_notify_data; /* read-only for driver */
	uint16 queue_reset;	  /* read-write */
};

/* BAR-relative regions; no CPU mapping or BAR assignment is implied. */
struct virtio_pci_regions {
	struct virtio_pci_cap common;
	struct virtio_pci_cap notify;
	struct virtio_pci_cap isr;
	struct virtio_pci_cap device;
	uint32 notify_off_multiplier;
	uint8 valid;
	uint8 mapped;
	uint64 common_va;
	uint64 notify_va;
	uint64 isr_va;
	uint64 device_va;
};

struct pci_device;
extern struct virtio_pci_regions virtio_blk_pci_regions;

const char *virtio_pci_cap_type_name(int cfg_type);
int virtio_pci_blk_smoke_test(struct pci_device *device,
			      const struct virtio_pci_regions *regions);

#endif
