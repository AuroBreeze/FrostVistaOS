#ifndef __MM_SLAB_H__
#define __MM_SLAB_H__

#include "kernel/types.h"
#include "kernel/spinlock.h"
#include "kernel/list.h"

#define ALIGN_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))

// when allocating a slab and there are no memory, we can sleep or not
#define KM_SLEEP 0
#define KM_NOSLEEP 1

/* Representation of a free node inside an unallocated object (Inline Freelist)
 */
struct kmem_bufctl {
	struct kmem_bufctl *next;
};

/* Represents a contiguous chunk of pages (Slab) */
// sizeof(kmem_slab) = 48B
struct kmem_slab {
	void *mem;	   /* Base address of memory in this slab */
	uint32 total_objs; /* Total object count in this slab include unused
			      objects */
	uint32 free_objs;  /* Current free object count */

	struct kmem_cache *cache; /* Owner cache (used by unified kfree) */

	struct kmem_bufctl *freelist; /* Head of free objects in this slab */
	struct list_head
	    list; /* Node for linking inside kmem_cache's slab lists */
};

/* Represents a cache for a specific object size or type */
struct kmem_cache {
	char name[32];	      /* Name of the cache */
	uint64 obj_size;      /* Size of an individual object */
	uint64 slab_size;     /* Total size of one slab in bytes */
	uint64 total_size;    /* Total size of all slabs in bytes */
	int align;	      /* Alignment requirement */
	struct spinlock lock; /* Lock for this cache */

	void (*constructor)(void *, uint64); /* Object constructor */
	void (*destructor)(void *, uint64);  /* Object destructor */

	/* Slabs managed by state */
	struct list_head slabs_full;	/* Fully allocated slabs */
	struct list_head slabs_partial; /* Partially allocated slabs */
	struct list_head slabs_empty;	/* Fully free slabs */

	struct list_head cache_list;
};

struct slab_cache {
	struct list_head cache_list;
	struct spinlock lock; /* Lock for this cache_list */
};

extern struct slab_cache slab_cache;

void slab_init(void);
struct kmem_cache *kmem_cache_create(char *name, uint64 obj_size, int align,
				     void (*constructor)(void *, uint64),
				     void (*destructor)(void *, uint64));
int kmem_cache_grow(struct kmem_cache *cp);
int kmem_cache_reap(struct kmem_cache *cp);
void *kmem_cache_alloc(struct kmem_cache *cp, int flags);
void kmem_cache_free(struct kmem_cache *cp, void *buf);
void kmem_cache_destroy(struct kmem_cache *cp);

#endif
