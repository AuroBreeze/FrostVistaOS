#ifndef __MM_KMALLOC_H__
#define __MM_KMALLOC_H__

#include "kernel/types.h"

/* Max slab-bucket size for the size-class buckets (2-power slots 8..2048, 9
 * buckets) */
#define KMALLOC_MAX_SLAB_SIZE 2048

/*
 * kmalloc - allocate size bytes of memory.
 *
 * size <= 2048  -> allocate from a 2-power size-class bucket (slab)
 * 2048 < size <= PGSIZE - sizeof(struct kmem_slab) (currently 4048)
 *             -> allocate a whole page, marked with a fake kmem_slab
 *                (cache == NULL) at the page tail
 * size larger  -> unsupported (no multi-page allocator yet), returns 0
 *
 * Returns 8-byte aligned memory, or 0 on failure. kmalloc_cache_init() must
 * have been called first.
 */
void *kmalloc(uint64 size);

/*
 * kmfree - release memory allocated by kmalloc() or any slab cache object.
 *
 * Distinguishes the source via the kmem_slab.cache at the page tail:
 *   cache != NULL -> kmem_cache_free(cache, ptr)
 *   cache == NULL -> kfree(ptr) (whole-page object)
 */
void kmfree(void *ptr);

/* Initialize the 9 size-class buckets (8..2048). Call after slab_init();
 * safe to call repeatedly. */
int kmalloc_cache_init(void);

#endif
