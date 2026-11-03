#ifdef CONFIG_SLAB_BASELINE_TEST
#define LOG_MODULE "SLAB_BASELINE_TEST"

#include <kernel/types.h>
#include <kernel/log.h>
#include <kernel/arch/timer.h>
#include <kernel/defs.h>
#include <kernel/mm/slab.h>
#include <asm/mm.h>

#define HOT_ROUND 900000

uint64 read_time_us()
{
	uint64 t;
	asm volatile("" ::: "memory");
	t = arch_read_time_us();
	asm volatile("" ::: "memory");
	return t;
}

static void output_padded(const char *str, int width)
{
	int len = 0;
	kprintf("%s", str);
	while (str[len])
		len++;
	for (; len < width; len++)
		kprintf(" ");
}

void output_cell(char *str)
{
	kprintf(",");
	output_padded(str, 12);
}

void output_uint64(uint64 num)
{
	char buf[21];
	int pos = sizeof(buf) - 1;

	buf[pos] = '\0';
	do {
		buf[--pos] = '0' + (num % 10);
		num /= 10;
	} while (num);

	output_cell(&buf[pos]);
}

static void output_header_unit(char *name, char *unit)
{
	output_padded(name, 22);
	output_cell("sample");
	output_cell("size_bytes");
	output_cell("round");
	output_cell(unit);
	output_cell("total_us");
	output_cell("start_page");
	output_cell("end_page");
	kprintf("\n");
}

void output_header(char *name)
{
	output_header_unit(name, "avg_pair_ns");
}

void output_result(char *name, uint64 round, int sample, int size,
		   uint64 avg_ns, uint64 total_us, uint64 start_page,
		   uint64 end_page)
{
	output_padded(name, 22);
	output_uint64(sample);
	output_uint64(size);
	output_uint64(round);
	output_uint64(avg_ns);
	output_uint64(total_us);
	output_uint64(start_page);
	output_uint64(end_page);
	kprintf("\n");
}

struct kmem_cache *create_cache(uint64 size)
{
	return kmem_cache_create("kmalloc", size, 0, 0, 0);
}

int slab_hot_baseline_test(uint64 size)
{
	uint64 start;
	uint64 end;
	uint64 total_time = 0;
	struct kmem_cache *cache = create_cache(size);
	if (!cache) {
		LOG_WARN("create_cache failed");
		return -1;
	}
	if (!kmem_cache_grow(cache)) {
		LOG_WARN("kmem_cache_grow failed");
		kmem_cache_destroy(cache);
		return -1;
	}

	/* Keep one object live, but prepare a second slot before timing. */
	void *pre_obj = kmem_cache_alloc(cache, KM_NOSLEEP);
	if (!pre_obj) {
		kmem_cache_destroy(cache);
		return -1;
	}
	void *warm_obj = kmem_cache_alloc(cache, KM_SLEEP);
	if (!warm_obj) {
		kmem_cache_free(cache, pre_obj);
		kmem_cache_destroy(cache);
		return -1;
	}
	kmem_cache_free(cache, warm_obj);

	output_header("HOT ALLOCATE NOSLEEP");
	for (int i = 0; i < 10; i++) {
		uint64 start_page = cache->total_size / PGSIZE;

		start = read_time_us();
		uint64 completed;
		for (completed = 0; completed < HOT_ROUND; completed++) {
			void *obj = kmem_cache_alloc(cache, KM_NOSLEEP);
			if (!obj) {
				break;
			}
			kmem_cache_free(cache, obj);
		}
		end = read_time_us();
		if (completed != HOT_ROUND) {
			LOG_WARN("hot baseline: allocation failed; sample "
				 "discarded");
			kmem_cache_free(cache, pre_obj);
			kmem_cache_destroy(cache);
			return -1;
		}

		uint64 end_page = cache->total_size / PGSIZE;
		total_time = end - start;

		output_result("hot data alloc", completed, i, size,
			      (total_time * 1000) / completed, total_time,
			      start_page, end_page);
	}

	kmem_cache_free(cache, pre_obj);
	kmem_cache_destroy(cache);
	return 0;
}

int slab_stream_baseline_test(uint64 size)
{
	struct kmem_cache *cache = create_cache(size);
	if (!cache) {
		LOG_WARN("create_cache failed");
		return -1;
	}
	if (!kmem_cache_grow(cache)) {
		LOG_WARN("kmem_cache_grow failed");
		kmem_cache_destroy(cache);
		return -1;
	}

	/* Keep pointers outside the slab and off the small kernel stack. */
	void **objs = (void **) kalloc();
	if (!objs) {
		kmem_cache_destroy(cache);
		return -1;
	}
	/* This private cache has exactly one slab; use its actual capacity. */
	struct kmem_slab *slab =
	    container_of(cache->slabs_empty.next, struct kmem_slab, list);
	uint32 count = slab->total_objs;
	if (!count || count > PGSIZE / sizeof(*objs)) {
		LOG_WARN("stream baseline: invalid slab capacity");
		kfree((void *) objs);
		kmem_cache_destroy(cache);
		return -1;
	}

	uint64 batches = (HOT_ROUND + count - 1) / count;
	uint64 operations = batches * count;
	/* Untimed warm-up also verifies exhaustion without growing the cache.
	 */
	uint32 warm_count;
	for (warm_count = 0; warm_count < count; warm_count++) {
		objs[warm_count] = kmem_cache_alloc(cache, KM_NOSLEEP);
		if (!objs[warm_count])
			break;
	}
	void *extra = kmem_cache_alloc(cache, KM_NOSLEEP);
	int warm_ok = warm_count == count && !extra && !slab->free_objs;
	if (extra)
		kmem_cache_free(cache, extra);
	for (uint32 i = 0; i < warm_count; i++)
		kmem_cache_free(cache, objs[i]);
	if (!warm_ok || slab->free_objs != count) {
		LOG_WARN("stream baseline: warm-up capacity check failed");
		kfree((void *) objs);
		kmem_cache_destroy(cache);
		return -1;
	}
	output_header_unit("STREAM NOSLEEP", "avg_op_ns");
	for (int sample = 0; sample < 10; sample++) {
		uint64 alloc_us = 0;
		uint64 free_us = 0;
		uint64 start_page = cache->total_size / PGSIZE;
		for (uint64 batch = 0; batch < batches; batch++) {
			uint64 start = read_time_us();
			uint32 allocated;
			for (allocated = 0; allocated < count; allocated++) {
				objs[allocated] =
				    kmem_cache_alloc(cache, KM_NOSLEEP);
				if (!objs[allocated])
					break;
			}
			uint64 end = read_time_us();
			alloc_us += end - start;
			if (allocated != count || slab->free_objs != 0 ||
			    slab->freelist != 0) {
				for (uint32 i = 0; i < allocated; i++)
					kmem_cache_free(cache, objs[i]);
				LOG_WARN("stream baseline: allocation failed");
				kfree((void *) objs);
				kmem_cache_destroy(cache);
				return -1;
			}

			start = read_time_us();
			for (uint32 i = 0; i < count; i++)
				kmem_cache_free(cache, objs[i]);
			end = read_time_us();
			free_us += end - start;
			/* Validate outside the timed ranges; free() has no
			 * return value. */
			if (slab->free_objs != count ||
			    !list_is_empty(&cache->slabs_full) ||
			    !list_is_empty(&cache->slabs_partial)) {
				LOG_WARN("stream baseline: slab not empty "
					 "after free");
				kfree((void *) objs);
				kmem_cache_destroy(cache);
				return -1;
			}
		}
		uint64 end_page = cache->total_size / PGSIZE;
		/* Accumulate microseconds before converting to average
		 * nanoseconds. */
		output_result("stream alloc", operations, sample, size,
			      alloc_us * 1000ULL / operations, alloc_us,
			      start_page, end_page);
		output_result("stream free", operations, sample, size,
			      free_us * 1000ULL / operations, free_us,
			      start_page, end_page);
	}
	kfree((void *) objs);
	kmem_cache_destroy(cache);
	return 0;
}

int slab_baseline_test()
{
	int sizes[7] = {8, 32, 64, 128, 256, 1024, 2048};
	for (int i = 0; i < 7; i++) {
		// if (slab_hot_baseline_test(sizes[i]) != 0)
		// 	return -1;
		if (slab_stream_baseline_test(sizes[i]) != 0)
			return -1;
	}

	return 0;
}

#endif
