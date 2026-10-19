#ifndef FV_DRIVER_PCI_RESOURCE_H
#define FV_DRIVER_PCI_RESOURCE_H

#include "kernel/types.h"
struct pci_device;
struct pci_bar;

int pci_probe_bar(uint64 cfg_addr, struct pci_device *device);
int pci_collect_resources(struct pci_device *target);
uint64 pci_memory_conflict(uint64 base, uint64 size,
			   const struct pci_bar *self);
int pci_assign_memory_bars(struct pci_device *device);

#endif
