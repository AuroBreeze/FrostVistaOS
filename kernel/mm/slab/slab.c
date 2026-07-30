#include "slab.h"
#include "asm/mm.h"
#include "kernel/defs.h"
#include "kernel/list.h"
#include "kernel/log.h"
#include "kernel/types.h"

struct slab_cache slab_cache = {0};

void slab_init(void)
{
	list_init(&slab_cache.cache_list);
}

/**
 * kmem_cache_create - create a new slab cache
 *
 * @name: name of the slab cach
 * @obj_size: size of a single object
 * */
struct kmem_cache *kmem_cache_create(char *name, uint64 obj_size, int align,
				     void (*constructor)(void *, uint64),
				     void (*destructor)(void *, uint64))
{
	struct kmem_cache *cache = (struct kmem_cache *) kalloc();
	if (!cache)
		return 0;
	if (obj_size < 8) {
		// a pointer size
		obj_size = 8;
	}
	if (align % 2 != 0) {
		LOG_WARN("kmem_cache_create: align must be power of 2");
		return 0;
	}
	if (align == 0) {
		align = 8;
	}

	obj_size = ALIGN_UP(obj_size, align);

	cache->obj_size = obj_size;
	cache->align = align;
	cache->constructor = constructor;
	cache->destructor = destructor;
	cache->total_size = 0;

	strncpy(cache->name, name, sizeof(cache->name) - 1);
	cache->name[sizeof(cache->name) - 1] = '\0';

	initlock(&cache->lock, cache->name);

	list_init(&cache->cache_list);
	list_init(&cache->slabs_full);
	list_init(&cache->slabs_partial);
	list_init(&cache->slabs_empty);

	list_add_tail(&cache->cache_list, &slab_cache.cache_list);

	return cache;
}

int kmem_cache_grow(struct kmem_cache *cp)
{
	void *new_space;
	if ((new_space = kalloc()) == 0) {
		LOG_WARN("kmem_cache_grow: kalloc failed");
		return 0;
	}

	// place the slab in the page tail
	// the paper points out that the page space utilization rate does not
	// fully exhuast the available space, and the remaining space can be
	// used to store data structures.
	struct kmem_slab *slab =
	    (struct kmem_slab *) (((uint64) new_space | (PGSIZE - 1)) -
				  sizeof(struct kmem_slab));

	list_init(&slab->list);

	slab->mem = new_space;
	slab->total_objs = (PGSIZE - sizeof(struct kmem_slab)) / cp->obj_size;
	if (slab->total_objs == 0) {
		LOG_WARN("kmem_cache_grow: slab->total_objs == 0");
		kfree(new_space);
		return 0;
	}
	slab->free_objs = slab->total_objs;

	// PERF: slab color need to be considered
	// int remain = (PGSIZE - sizeof(struct kmem_slab)) % cp->obj_size;

	struct kmem_bufctl *head = 0;
	for (int i = 0; i < slab->total_objs; i++) {
		struct kmem_bufctl *node =
		    (struct kmem_bufctl *) ((char *) new_space +
					    (i * cp->obj_size));

		if (cp->constructor)
			cp->constructor(node, cp->obj_size);
		node->next = head;
		head = node;
	}
	slab->freelist = head;

	acquire(&cp->lock);
	cp->total_size += PGSIZE;
	list_add_tail(&slab->list, &cp->slabs_empty);
	release(&cp->lock);

	return PGSIZE;
}

void *kmem_cache_alloc(struct kmem_cache *cp, int flags)
{
	if (!cp)
		return 0;

	struct list_head *head = 0;

	int need_grow = 0;

	if (list_is_empty(&cp->slabs_partial) &&
	    list_is_empty(&cp->slabs_empty)) {
		if (kmem_cache_grow(cp) == 0)
			return 0;
		need_grow = 1;
	}

	acquire(&cp->lock);
	if (need_grow) {
		head = cp->slabs_empty.next;
	} else {
		if (!list_is_empty(&cp->slabs_partial)) {
			head = cp->slabs_partial.next;
		} else {
			head = cp->slabs_empty.next;
		}
	}

	struct kmem_slab *slab = container_of(head, struct kmem_slab, list);

	if (slab->free_objs == 0)
		return 0;

	struct kmem_bufctl *node = slab->freelist;
	void *ret = node;
	slab->freelist = node->next;

	int was_empty = (slab->free_objs == slab->total_objs);
	slab->free_objs--;

	if (was_empty && slab->free_objs != 0) {
		list_del(&slab->list);
		list_add_tail(&slab->list, &cp->slabs_partial);
	}
	if (slab->free_objs == 0) {
		list_del(&slab->list);
		list_add_tail(&slab->list, &cp->slabs_full);
	}
	release(&cp->lock);

	return ret;
}

void kmem_cache_free(struct kmem_cache *cp, void *buf)
{
	if (!cp || !buf)
		return;
	acquire(&cp->lock);

	// Retrieve the slab management unit at the end of the page
	struct kmem_slab *slab =
	    (struct kmem_slab *) (((uint64) buf | (PGSIZE - 1)) -
				  sizeof(struct kmem_slab));

	int was_full = (slab->free_objs == 0);

	slab->free_objs++;
	if (slab->free_objs > slab->total_objs) {
		release(&cp->lock);
		panic("kmem_cache_free: slab->free_objs > slab->total_objs");
	}
	struct kmem_bufctl *node = (struct kmem_bufctl *) buf;
	node->next = slab->freelist;
	slab->freelist = node;

	int will_empty = (slab->free_objs == slab->total_objs);

	if (was_full) {
		list_del(&slab->list);
		list_add_tail(&slab->list, &cp->slabs_partial);
		release(&cp->lock);
		return;
	}
	if (will_empty) {
		list_del(&slab->list);
		list_add_tail(&slab->list, &cp->slabs_empty);
		release(&cp->lock);
		return;
	}
	release(&cp->lock);
};
