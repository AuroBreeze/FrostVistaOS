#ifndef FV_LOONGARCH_PICE_ECAM_H
#define FV_LOONGARCH_PICE_ECAM_H

#include "asm/machine.h"
#include "kernel/types.h"

#define PCIE_ECAM_BASE 0x20000000UL
#define PCIE_ECAM_PAGE_PA (PCIE_ECAM_BASE & ~(0x1000ULL - 1ULL))
#define PCIE_ECAM_PAGE_VA (KERNEL_IO_BASE + PCIE_ECAM_PAGE_PA)
#define PCIE_ECAM_HIGH_BASE (KERNEL_IO_BASE + PCIE_ECAM_BASE)

// loongarch dts data
#define PCIE_ECAM_RANGE 0x8000000

#define PCIE_ECAM_BUS_SHIFT (20)  // Bus Number A[(20+n-1):20]
#define PCIE_ECAH_DEV_SHIFT (15)  // Device Number A[19:15] 32
#define PCIE_ECAM_FUNC_SHIFT (12) // Function Number A[14:12]

// total devices number
#define PCI_MAX_DEVICES 32

#define pcie_read8(offset) (*(volatile uint8 *) (PCIE_ECAM_HIGH_BASE + offset))
#define pcie_read16(offset)                                                    \
	(*(volatile uint16 *) (PCIE_ECAM_HIGH_BASE + offset))
#define pcie_read32(offset)                                                    \
	(*(volatile uint32 *) (PCIE_ECAM_HIGH_BASE + offset))
#define pcie_read64(offset)                                                    \
	(*(volatile uint64 *) (PCIE_ECAM_HIGH_BASE + offset))

#define INVALID_ID 0xffff
#define VENDOR_OFFSET 0x00
#define DEVICE_OFFSET 0x02
#define REVISION_ID_OFFSET 0x08
#define CLASS_CODE_OFFSET 0x09
#define HEADER_TYPE_OFFSET 0x0E

struct pci_device {
	uint8 bus;
	uint8 dev;
	uint8 func;
	uint8 header_type;

	uint16 vendor;
	uint16 device;

	uint32 class_code;
};

extern struct pci_device pci_devices[PCI_MAX_DEVICES];
extern uint pci_device_count;

void pcie_enumerate();

#endif
