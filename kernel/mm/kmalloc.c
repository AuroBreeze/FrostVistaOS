// kmalloc: size-class based general purpose allocator on top of slab.
//
// Layout:
//   size <= 2048            -> 2-power size-class kmem_cache bucket (9 buckets)
//   2048 < size <= 4048     -> page allocator, page-tail marked with a fake
//                              kmem_slab (cache == 0)
//   size > 4048             -> unsupported (no multi-page allocator yet)
//
// kfree_any() distinguishes the two sources by reading the kmem_slab at the
// page tail: cache != 0 means a slab object, cache == 0 means a whole page.

#define LOG_MODULE "KMALLOC"

#include "kernel/arch/mm.h"
#include "kernel/defs.h"
#include "kernel/mm/kalloc.h"
#include "kernel/mm/kmalloc.h"
#include "kernel/log.h"
#include "kernel/mm/slab.h"

#define KMALLOC_NUM_CACHES 9 /* 8,16,...,2048 */
static struct kmem_cache *kmalloc_caches[KMALLOC_NUM_CACHES];
static int kmalloc_ready = 0;

int kmalloc_cache_init(void)
{
	if (kmalloc_ready)
		return 0;

	int i;
	int idx = 0;
	for (i = 8; i <= KMALLOC_MAX_SLAB_SIZE; i *= 2) {
		kmalloc_caches[idx] = kmem_cache_create("kmalloc", i, 0, 0, 0);
		if (kmalloc_caches[idx] == 0)
			LOG_WARN("kmalloc_cache_init: create cache size %d "
				 "failed",
				 i);
		idx++;
	}

	kmalloc_ready = 1;
	LOG_INFO("kmalloc init: %d size-class caches", idx);
	return 0;
}

void *kmalloc(uint64 size)
{
	if (!kmalloc_ready)
		kmalloc_cache_init();

	if (size <= KMALLOC_MAX_SLAB_SIZE) {
		int idx = 0;
		while ((8 << idx) < size)
			idx++;

		struct kmem_cache *cp = kmalloc_caches[idx];
		if (!cp)
			return 0;
		void *p = kmem_cache_alloc(cp, KM_SLEEP);
		if (!p)
			return 0;
		memset(p, 0, 8 << idx);
		return p;
	}

	/* Large object: whole page, keep the page-tail sizeof(kmem_slab) free
	 * so that kfree_any() can find the fake kmem_slab marker. */
	if (size <= PGSIZE - sizeof(struct kmem_slab)) {
		void *p = kalloc();
		if (!p)
			return 0;

		struct kmem_slab *slab =
		    (struct kmem_slab *) ((uint64) p + PGSIZE -
					  sizeof(struct kmem_slab));
		slab->cache = 0; /* marker: whole-page object */
		return p;
	}

	LOG_WARN("kmalloc: size %d too large, unsupported", size);
	return 0;
}

void kmfree(void *ptr)
{
	if (!ptr)
		return;

	if ((uint64) ptr < (uint64) _kernel_end)
		panic("kfree_any: bad pointer");

	/* kmem_slab sits at the tail of the page containing ptr */
	struct kmem_slab *slab =
	    (struct kmem_slab *) (((uint64) ptr | (PGSIZE - 1)) + 1 -
				  sizeof(struct kmem_slab));

	if (slab->cache)
		kmem_cache_free(slab->cache, ptr);
	else
		kfree(ptr); /* whole-page object (page-aligned) */
}
