#ifndef FV_KERNEL_VMA_H
#define FV_KERNEL_VMA_H

#include "kernel/list.h"
#include "kernel/sleeplock.h"
#include "kernel/types.h"

#define NVMA 16 // Number of virtual memory areas

// linux 2.6.4
#define PROT_READ 0x1  /* page can be read */
#define PROT_WRITE 0x2 /* page can be written */
#define PROT_EXEC 0x4  /* page can be executed */
#define PROT_SEM 0x8   /* page may be used for atomic ops */
#define PROT_NONE 0x0  /* page can not be accessed */

// linux 2.6.4
#define MAP_SHARED 0x01	   /* Share changes */
#define MAP_PRIVATE 0x02   /* Changes are private */
#define MAP_TYPE 0x0f	   /* Mask for type of mapping */
#define MAP_FIXED 0x10	   /* Interpret addr exactly */
#define MAP_ANONYMOUS 0x20 /* don't use a file */

// Created via slab allocation; requires separate deallocation.
struct mapped_vma {
	struct Process *proc;
	struct vm_area_struct *vma;

	// Mounted into the `mapped_head` within the `address_space`
	struct list_head head;
};

struct cached_file_page {
	uint64 index;
	uint64 kva;
	struct list_head link;
};

struct address_space {
	struct inode *host;
	struct list_head cached_pages;

	// mapped_vma Linked list
	struct list_head mapped_head;
	// protect address space
	struct sleeplock sleeplock;
};

/* Hold the mapping lock while looking up or clearing cached pages.
 * The final inode owner can clear the cache after detaching the mapping. */
struct cached_file_page *mapping_find_page(struct address_space *mapping,
					   uint64 index);
void mapping_clear_pages(struct address_space *mapping);

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
