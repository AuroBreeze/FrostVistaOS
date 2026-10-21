#ifndef FV_DRIVER_PCI_H
#define FV_DRIVER_PCI_H

#include "kernel/types.h"
#include "platform/pcie_ecam.h"

#define PCIE_ECAM_BUS_SHIFT (20)  // Bus Number A[(20+n-1):20]
#define PCIE_ECAH_DEV_SHIFT (15)  // Device Number A[19:15] 32
#define PCIE_ECAM_FUNC_SHIFT (12) // Function Number A[14:12] 8

#define INVALID_ID 0xffff
#define VENDOR_OFFSET 0x00
#define DEVICE_OFFSET 0x02
#define COMMAND_OFFSET 0x04
#define STATUS_OFFSET 0x06
#define REVISION_ID_OFFSET 0x08
#define CLASS_CODE_OFFSET 0x09
#define BAR_OFFSET 0x10
#define HEADER_TYPE_OFFSET 0x0E
#define CAPABILITIES_OFFSET 0x34
#define INTERRUPT_PIN_OFFSET 0x3D // type 0

// 0x09; Identifies a vendor-specific capability.
#define PCI_CAP_ID_VENDOR 0x09
#define PCI_CONFIG_SPACE_SIZE 256
// Status register
#define PCI_STATUS_CAP_LIST (1U << 4)
// Command register
#define PCI_COMMAND_IO (1U << 0)
#define PCI_COMMAND_MEMORY (1U << 1)
#define PCI_COMMAND_MASTER (1U << 2)
#define PCI_COMMAND_INTX_DISABLE (1U << 10)

// total devices number
#define PCI_MAX_DEVICES 32
// total functions number
#define PCI_MAX_FUNCTIONS 8

#define pcie_read8(offset)                                                     \
	(*(volatile uint8 *) (ARCH_PCIE_ECAM_HIGH_BASE + offset))
#define pcie_read16(offset)                                                    \
	(*(volatile uint16 *) (ARCH_PCIE_ECAM_HIGH_BASE + offset))
#define pcie_read32(offset)                                                    \
	(*(volatile uint32 *) (ARCH_PCIE_ECAM_HIGH_BASE + offset))
#define pcie_read64(offset)                                                    \
	(*(volatile uint64 *) (ARCH_PCIE_ECAM_HIGH_BASE + offset))

#define pcie_write8(offset, value)                                             \
	(*(volatile uint8 *) (ARCH_PCIE_ECAM_HIGH_BASE + offset) = value)
#define pcie_write16(offset, value)                                            \
	(*(volatile uint16 *) (ARCH_PCIE_ECAM_HIGH_BASE + offset) = value)
#define pcie_write32(offset, value)                                            \
	(*(volatile uint32 *) (ARCH_PCIE_ECAM_HIGH_BASE + offset) = value)
#define pcie_write64(offset, value)                                            \
	(*(volatile uint64 *) (ARCH_PCIE_ECAM_HIGH_BASE + offset) = value)

struct pci_bar {
	uint64 base;
	uint64 size;
	uint8 valid;
	uint8 is_io;
	uint8 is_64;
	uint8 prefetchable;
};

struct pci_device {
	uint8 bus;
	uint8 dev;
	uint8 func;
	uint8 header_type;

	uint16 vendor;
	uint16 device;

	uint32 class_code;
	uint8 interrupt_pin;
	struct pci_bar bars[6];
};

extern struct pci_device pci_devices[PCI_MAX_DEVICES];
extern uint pci_device_count;

void pcie_enumerate();
struct pci_device *pci_find_device(uint64 vendor, uint64 device);
void pci_restore_command(uint16 saved_command, uint64 cfg_addr);
int pci_disable_decoding(uint64 cfg_addr, uint16 *saved_command);

#endif
