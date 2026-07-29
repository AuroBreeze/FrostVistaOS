#ifndef __MM_SLAB_H__
#define __MM_SLAB_H__

#include "kernel/types.h"
#include "kernel/spinlock.h"

#define offsetof(type, member) ((uint64) & (((type *) 0)->member))
#define container_of(ptr, type, member)                                        \
	((type *) ((char *) ptr - offsetof(type, member)))

// assume new_node and head are (struct list_head *)
#define list_add_tail(new_node, head)                                          \
	do {                                                                   \
		struct list_head *_new = (new_node);                           \
		struct list_head *_head = (head);                              \
		_new->next = _head;                                            \
		_new->prev = _head->prev;                                      \
		_head->prev->next = _new;                                      \
		_head->prev = _new;                                            \
	} while (0)

// assume head is (struct list_head *)
#define list_is_empty(head) ((struct list_head *) (head)->next == (head))
#define list_first(head) ((struct list_head *) (head)->next)
#define list_del(head)                                                         \
	do {                                                                   \
		struct list_head *_head = (head);                              \
		struct list_head *_next = _head->next;                         \
		_head->prev->next = _next;                                     \
		_next->prev = _head->prev;                                     \
		_head->next = 0;                                               \
		_head->prev = 0;                                               \
	} while (0)

// when allocating a slab and there are no memory, we can sleep or not
#define KM_SLEEP 0
#define KM_NOSLEEP 1

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

/* Representation of a free node inside an unallocated object (Inline Freelist)
 */
struct kmem_bufctl {
	struct kmem_bufctl *next;
};

/* Represents a contiguous chunk of pages (Slab) */
// sizeof(kmem_slab) = 40B
struct kmem_slab {
	void *mem;	   /* Base address of memory in this slab */
	uint32 total_objs; /* Total object count in this slab include unused
			      objects */
	uint32 free_objs;  /* Current free object count */

	struct kmem_bufctl *freelist; /* Head of free objects in this slab */
	struct list_head
	    list; /* Node for linking inside kmem_cache's slab lists */
};

/* Represents a cache for a specific object size or type */
struct kmem_cache {
	char name[32];	      /* Name of the cache */
	uint64 obj_size;      /* Size of an individual object */
	uint64 slab_size;     /* Total size of one slab in bytes */
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
};
#endif
