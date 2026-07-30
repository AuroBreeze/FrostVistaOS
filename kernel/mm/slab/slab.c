#define LOG_MODULE "SLAB"

#include "slab.h"
#include "asm/mm.h"
#include "kernel/defs.h"
#include "kernel/list.h"
#include "kernel/log.h"
#include "kernel/types.h"

struct slab_cache slab_cache = {0};
static int slab_cache_init = 0;

static int is_power_of_two(uint64 x)
{
	return x && !(x & (x - 1));
}

void slab_init(void)
{
	if (slab_cache_init == 1) {
		return;
	}
	LOG_INFO("slab init");
	list_init(&slab_cache.cache_list);
	initlock(&slab_cache.lock, "slab_cache");
	slab_cache_init = 1;
}

/**
 * kmem_cache_create - create a new slab cache
 *
 * @name: name of the slab cach
 * @obj_size: size of a single object
 *
 * Lock Contract:
 *  cannot be called while holding slab_cache.lock
 * */
struct kmem_cache *kmem_cache_create(char *name, uint64 obj_size, int align,
				     void (*constructor)(void *, uint64),
				     void (*destructor)(void *, uint64))
{
	// kmem_cache_destroy will free the memory
	struct kmem_cache *cache = (struct kmem_cache *) kalloc();
	if (!cache)
		return 0;
	if (obj_size < 8) {
		// a pointer size
		LOG_DEBUG(
		    "kmem_cache_create: obj_size must be at least 8 bytes");
		obj_size = 8;
	}
	if (align != 0 && !is_power_of_two(align)) {
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

	acquire(&slab_cache.lock);
	list_add_tail(&cache->cache_list, &slab_cache.cache_list);
	release(&slab_cache.lock);

	return cache;
}

/**
 * kmem_cache_grow - create a new slab cache and allocate a new PAGE
 *
 * Lock contract:
 *  cannot be called when holding the cache lock
 *  grow will acquire the cache lock
 *
 * Return: PGSIZE if success else 0
 * */
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

/**
 * kmem_cache_reap - free a page from the kmem_cach.slabs_empty
 *
 * Lock contract:
 *  cannot be called while holding cp->lock
 * */
int kmem_cache_reap(struct kmem_cache *cp)
{
	if (!cp) {
		LOG_WARN("kmem_cache_reap: cp is null");
		return -1;
	}

	acquire(&cp->lock);
	if (list_is_empty(&cp->slabs_empty)) {
		LOG_WARN(
		    "kmem_cache_reap: kmem_cach slabs_empty list is empty");
		release(&cp->lock);
		return -1;
	}

	struct kmem_slab *slab =
	    container_of(cp->slabs_empty.next, struct kmem_slab, list);
	if (slab->free_objs != slab->total_objs) {
		LOG_WARN(
		    "kmem_cache_reap: slab->free_objs != slab->total_objs");
		release(&cp->lock);
		return -1;
	}

	list_del(&slab->list);

	void *mem = slab->mem;
	cp->total_size -= PGSIZE;

	release(&cp->lock);
	for (int i = 0; i < slab->total_objs; i++) {
		struct kmem_bufctl *node =
		    (struct kmem_bufctl *) ((char *) mem + (i * cp->obj_size));
		if (cp->destructor)
			cp->destructor(node, cp->obj_size);
	}
	kfree(mem);
	return 0;
}

/**
 * kmem_cache_alloc - allocate a slab object from the slab cache
 *
 * Lock contract:
 *  cannot be called while holding cp->lock
 * */
void *kmem_cache_alloc(struct kmem_cache *cp, int flags)
{
	if (!cp)
		return 0;

	struct list_head *head = 0;
	int need_grow = 0;

	acquire(&cp->lock);
	if (list_is_empty(&cp->slabs_partial) &&
	    list_is_empty(&cp->slabs_empty)) {
		if (flags == KM_NOSLEEP) {
			release(&cp->lock);
			return 0;
		}

		release(&cp->lock);
		if (flags == KM_SLEEP && kmem_cache_grow(cp) == 0)
			return 0;
		acquire(&cp->lock);
		need_grow = 1;
	}
	release(&cp->lock);

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

	if (slab->free_objs == 0) {
		release(&cp->lock);
		return 0;
	}

	struct kmem_bufctl *node = slab->freelist;
	void *ret = node;
	slab->freelist = node->next;

	int was_empty = (slab->free_objs == slab->total_objs);
	slab->free_objs--;

	if (was_empty) {
		if (slab->free_objs) {
			list_del(&slab->list);
			list_add_tail(&slab->list, &cp->slabs_partial);
		} else {
			list_del(&slab->list);
			list_add_tail(&slab->list, &cp->slabs_full);
		}
	} else if (slab->free_objs == 0) {
		list_del(&slab->list);
		list_add_tail(&slab->list, &cp->slabs_full);
	}

	release(&cp->lock);
	return ret;
}

/**
 * kmem_cache_free - free a slab object
 *
 * Lock Contract:
 *  cannot be called while holding cp->lock
 * */
void kmem_cache_free(struct kmem_cache *cp, void *buf)
{
	if (!cp || !buf)
		return;
	acquire(&cp->lock);

	// Retrieve the slab management unit at the end of the page
	struct kmem_slab *slab =
	    (struct kmem_slab *) (((uint64) buf | (PGSIZE - 1)) -
				  sizeof(struct kmem_slab));
	if ((uint64) buf < (uint64) slab->mem ||
	    (uint64) buf >= (uint64) slab->mem + PGSIZE)
		panic("kmem_cache_free: buf is not in slab");

	struct kmem_bufctl *tmp = 0;
	for (tmp = slab->freelist; tmp; tmp = tmp->next) {
		if (tmp == buf) {
			release(&cp->lock);
			LOG_WARN("kmem_cache_free: double free");
			return;
		}
	}

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

/**
 * kmem_cache_destroy - Destroy a slab cache and free all allocated memory
 *
 * Lock Contract:
 *  cannot be called while holding cp->lock and slab_cache.lock
 * */
void kmem_cache_destroy(struct kmem_cache *cp)
{
	if (!cp) {
		LOG_WARN("kmem_cache_destroy: cp is null");
		return;
	}

	acquire(&cp->lock);
	if (!list_is_empty(&cp->slabs_full) ||
	    !list_is_empty(&cp->slabs_partial)) {
		LOG_WARN("kmem_cache_destroy: slab list not empty");
		release(&cp->lock);
		return;
	}
	// BUG: UAF: lock will lost and other functions will access cp
	release(&cp->lock);

	while (1) {
		acquire(&cp->lock);
		if (list_is_empty(&cp->slabs_empty)) {
			release(&cp->lock);
			break;
		}
		struct kmem_slab *slab =
		    container_of(cp->slabs_empty.next, struct kmem_slab, list);
		if (slab->free_objs != slab->total_objs)
			panic("destroy non-empty slab");
		void *mem = slab->mem;
		list_del(&slab->list);
		if (cp->total_size < PGSIZE) {
			release(&cp->lock);
			panic("kmem_cache_destroy: cp->total_size < PGSIZE");
		}
		cp->total_size -= PGSIZE;

		release(&cp->lock);

		for (int i = 0; i < slab->total_objs; i++) {
			struct kmem_bufctl *node =
			    (struct kmem_bufctl *) ((char *) slab->mem +
						    (i * cp->obj_size));
			if (cp->destructor)
				cp->destructor(node, cp->obj_size);
		}
		kfree(mem);
	}

	acquire(&slab_cache.lock);
	list_del(&cp->cache_list);
	release(&slab_cache.lock);

	kfree(cp);
}
