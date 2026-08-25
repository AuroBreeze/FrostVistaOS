#ifndef __KERNEL_VMA_H__
#define __KERNEL_VMA_H__

#include "kernel/types.h"

#define NVMA 16 // Number of virtual memory areas

struct vm_area_struct {
	int used;
	uint64 va_start;
	uint64 va_end;

	uint64 flags;
	uint64 file_offset;
	uint64 vm_page_prot;
	struct vm_operations_struct *vm_ops;

	struct file *file;
	void *vm_private_data; // Pointer to private data
};

struct vm_operations_struct {
	void (*open)(struct vm_area_struct *vma);
	void (*close)(struct vm_area_struct *vma);
};

#endif
