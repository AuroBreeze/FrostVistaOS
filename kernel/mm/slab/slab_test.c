#define LOG_MODULE "SLAB"

#include "asm/mm.h"
#include "kernel/types.h"
#include "kernel/log.h"
#include "kernel/test.h"
#include "kernel/mm/slab.h"

/* -------------------------------------------------------------------------- */
/*  Helper: constructor / destructor counters for ctor/dtor tests             */
/* -------------------------------------------------------------------------- */

static int g_ctor_count = 0;
static int g_dtor_count = 0;

static void counting_ctor(void *buf, uint64 size)
{
	(void) buf;
	(void) size;
	g_ctor_count++;
}

static void counting_dtor(void *buf, uint64 size)
{
	(void) buf;
	(void) size;
	g_dtor_count++;
}

static void reset_counters(void)
{
	g_ctor_count = 0;
	g_dtor_count = 0;
}

/* -------------------------------------------------------------------------- */
/*  Helper: verify pointer alignment                                          */
/* -------------------------------------------------------------------------- */
static int is_aligned(void *ptr, int align)
{
	return ((uint64) ptr & (align - 1)) == 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: slab_init                                                           */
/* -------------------------------------------------------------------------- */
static int test_slab_init(void)
{
	slab_init();
	TEST_ASSERT(1, "slab_init succeeded");
	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_create  - basic */
/* -------------------------------------------------------------------------- */
static int test_slab_create(void)
{
	struct kmem_cache *c;

	c = kmem_cache_create("test_8", 8, 0, 0, 0);
	TEST_ASSERT(c != 0, "create 8-byte cache");
	TEST_ASSERT(c->obj_size == 8, "min obj_size == 8");

	c = kmem_cache_create("test_16", 16, 0, 0, 0);
	TEST_ASSERT(c != 0, "create 16-byte cache");

	c = kmem_cache_create("test_big", 1024, 0, 0, 0);
	TEST_ASSERT(c != 0, "create 1024-byte cache");

	c = kmem_cache_create("test_tiny", 4, 0, 0, 0);
	TEST_ASSERT(c != 0, "create 4-byte cache promoted to 8");
	TEST_ASSERT(c->obj_size == 8, "tiny obj_size promoted to 8");

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_create  - invalid align */
/* -------------------------------------------------------------------------- */
static int test_slab_create_bad_align(void)
{
	struct kmem_cache *c;

	c = kmem_cache_create("bad_align", 64, 12, 0, 0);
	TEST_ASSERT(c == 0, "align=12 is not power-of-2, should fail");

	c = kmem_cache_create("bad_align2", 64, 7, 0, 0);
	TEST_ASSERT(c == 0, "align=7 is not power-of-2, should fail");

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_create  - alignment */
/* -------------------------------------------------------------------------- */
static int test_slab_create_align(void)
{
	struct kmem_cache *c;

	c = kmem_cache_create("align_32", 128, 32, 0, 0);
	TEST_ASSERT(c != 0, "create cache with align=32");
	TEST_ASSERT(c->align == 32, "align preserved");
	TEST_ASSERT(c->obj_size == 128, "obj_size already aligned");

	c = kmem_cache_create("align_8", 13, 8, 0, 0);
	TEST_ASSERT(c != 0, "create cache with obj_size=13 align=8");
	TEST_ASSERT(c->obj_size == 16, "obj_size aligned up to 16");

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_create  - constructor / destructor */
/* -------------------------------------------------------------------------- */
static int test_slab_create_ctor_dtor(void)
{
	struct kmem_cache *c;

	c = kmem_cache_create("ctor_cache", 32, 0, counting_ctor,
			      counting_dtor);
	TEST_ASSERT(c != 0, "create cache with ctor/dtor");
	TEST_ASSERT(c->constructor == counting_ctor, "constructor set");
	TEST_ASSERT(c->destructor == counting_dtor, "destructor set");

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_create  - name */
/* -------------------------------------------------------------------------- */
static int test_slab_create_name(void)
{
	struct kmem_cache *c;

	c = kmem_cache_create("my_object_cache_32", 256, 0, 0, 0);
	TEST_ASSERT(c != 0, "create named cache");

	char long_name[40];
	for (int i = 0; i < 35; i++)
		long_name[i] = 'x';
	long_name[35] = '\0';
	c = kmem_cache_create(long_name, 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create cache with long name (truncated)");

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_alloc  - single allocation */
/* -------------------------------------------------------------------------- */
static int test_slab_alloc_single(void)
{
	struct kmem_cache *c;
	void *obj;

	c = kmem_cache_create("alloc_test", 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create alloc test cache");

	obj = kmem_cache_alloc(c, KM_SLEEP);
	TEST_ASSERT(obj != 0, "alloc one object");

	kmem_cache_free(c, obj);
	TEST_ASSERT(1, "free succeeded");

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_alloc  - allocate all objects in a slab */
/* -------------------------------------------------------------------------- */
static int test_slab_alloc_fill_slab(void)
{
	struct kmem_cache *c;
	void *objs[256];
	int n;
	int per_slab;

	c = kmem_cache_create("fill_test", 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create fill test cache");

	per_slab = (PGSIZE - sizeof(struct kmem_slab)) / (int) c->obj_size;
	TEST_ASSERT(per_slab >= 50, "first slab contains many objects");

	/* Fill exactly one slab */
	for (n = 0; n < per_slab; n++) {
		objs[n] = kmem_cache_alloc(c, KM_SLEEP);
		if (objs[n] == 0)
			break;
	}
	TEST_ASSERT(n == per_slab, "filled entire slab");

	/* Free them all */
	for (int i = 0; i < n; i++) {
		kmem_cache_free(c, objs[i]);
	}

	/* Re-allocate  - should succeed from cache (no grow needed) */
	void *r = kmem_cache_alloc(c, KM_NOSLEEP);
	TEST_ASSERT(r != 0,
		    "KM_NOSLEEP alloc after freeing all should succeed");
	kmem_cache_free(c, r);

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_alloc  - KM_NOSLEEP on empty cache */
/* -------------------------------------------------------------------------- */
static int test_slab_alloc_nosleep_empty(void)
{
	struct kmem_cache *c;
	void *obj;

	c = kmem_cache_create("nosleep_test", 128, 0, 0, 0);
	TEST_ASSERT(c != 0, "create nosleep test cache");

	/* Never grown  - should have no slabs */
	obj = kmem_cache_alloc(c, KM_NOSLEEP);
	TEST_ASSERT(obj == 0,
		    "KM_NOSLEEP on new cache returns NULL (no grow yet)");

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_alloc  - allocation alignment */
/* -------------------------------------------------------------------------- */
static int test_slab_alloc_alignment(void)
{
	struct kmem_cache *c;
	void *objs[16];

	c = kmem_cache_create("align_test", 128, 32, 0, 0);
	TEST_ASSERT(c != 0, "create align test cache");

	for (int i = 0; i < 16; i++) {
		objs[i] = kmem_cache_alloc(c, KM_SLEEP);
		TEST_ASSERT(objs[i] != 0, "alloc with alignment");
		TEST_ASSERT(is_aligned(objs[i], 32),
			    "object satisfies alignment");
	}

	for (int i = 0; i < 16; i++)
		kmem_cache_free(c, objs[i]);

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_alloc  - default 8-byte alignment */
/* -------------------------------------------------------------------------- */
static int test_slab_alloc_default_alignment(void)
{
	struct kmem_cache *c;
	void *objs[16];

	c = kmem_cache_create("def_align", 31, 0, 0, 0);
	TEST_ASSERT(c != 0, "create default-align cache");

	for (int i = 0; i < 16; i++) {
		objs[i] = kmem_cache_alloc(c, KM_SLEEP);
		TEST_ASSERT(objs[i] != 0, "alloc with default alignment");
		TEST_ASSERT(is_aligned(objs[i], 8),
			    "object satisfies default 8-byte alignment");
	}

	for (int i = 0; i < 16; i++)
		kmem_cache_free(c, objs[i]);

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_alloc  - alloc-free-alloc (reuse) */
/* -------------------------------------------------------------------------- */
static int test_slab_alloc_reuse(void)
{
	struct kmem_cache *c;
	void *a;
	void *b;

	c = kmem_cache_create("reuse_test", 256, 0, 0, 0);
	TEST_ASSERT(c != 0, "create reuse test cache");

	a = kmem_cache_alloc(c, KM_SLEEP);
	TEST_ASSERT(a != 0, "first alloc");

	kmem_cache_free(c, a);

	b = kmem_cache_alloc(c, KM_NOSLEEP);
	TEST_ASSERT(b != 0, "second alloc after free (no grow)");
	TEST_ASSERT(a == b, "same object reused (LIFO freelist)");

	kmem_cache_free(c, b);

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_alloc  - stress test */
/* -------------------------------------------------------------------------- */
static int test_slab_alloc_stress(void)
{
	struct kmem_cache *c;
	void *objs[128];
	int rounds = 8;
	/* 64-byte obj  - ~63 per slab; use 50 to stay within one slab */
	int batch = 50;

	c = kmem_cache_create("stress_test", 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create stress test cache");

	for (int r = 0; r < rounds; r++) {
		for (int i = 0; i < batch; i++) {
			objs[i] = kmem_cache_alloc(c, KM_SLEEP);
			TEST_ASSERT(objs[i] != 0, "stress alloc");
		}
		for (int i = 0; i < batch; i++) {
			*(uint64 *) objs[i] = (uint64) i + (r << 16);
		}
		for (int i = 0; i < batch; i++) {
			uint64 val = *(uint64 *) objs[i];
			TEST_ASSERT(val == (uint64) i + (r << 16),
				    "no corruption across rounds");
			kmem_cache_free(c, objs[i]);
		}
	}

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: constructor / destructor invocation                                 */
/* -------------------------------------------------------------------------- */
static int test_slab_ctor_dtor(void)
{
	struct kmem_cache *c;
	void *objs[16];

	reset_counters();

	c = kmem_cache_create("ctordtor", 64, 0, counting_ctor, counting_dtor);
	TEST_ASSERT(c != 0, "create ctor/dtor cache");

	/* First alloc triggers grow  - constructor runs per object */
	for (int i = 0; i < 8; i++) {
		objs[i] = kmem_cache_alloc(c, KM_SLEEP);
	}
	TEST_ASSERT(g_ctor_count > 0, "constructor was called during grow");

	/* Free all  - destructor NOT called on free (only on reap/destroy) */
	int ctor_before_free = g_ctor_count;
	int dtor_before_free = g_dtor_count;

	for (int i = 0; i < 8; i++) {
		kmem_cache_free(c, objs[i]);
	}
	TEST_ASSERT(g_ctor_count == ctor_before_free,
		    "ctor not called during free");
	TEST_ASSERT(g_dtor_count == dtor_before_free,
		    "dtor not called during free");

	/* Re-alloc  - no grow, no new ctor calls */
	for (int i = 0; i < 8; i++) {
		objs[i] = kmem_cache_alloc(c, KM_SLEEP);
	}
	TEST_ASSERT(g_ctor_count == ctor_before_free,
		    "ctor not called on reuse (already constructed)");

	/* Free objects and then destroy  - destructors should fire */
	for (int i = 0; i < 8; i++) {
		kmem_cache_free(c, objs[i]);
	}
	int dtor_before_destroy = g_dtor_count;

	kmem_cache_destroy(c);
	TEST_ASSERT(g_dtor_count > dtor_before_destroy,
		    "destructor called during destroy");
	TEST_ASSERT(g_dtor_count == g_ctor_count,
		    "constructor count == destructor count");

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_reap  - reap an empty slab */
/* -------------------------------------------------------------------------- */
static int test_slab_reap_basic(void)
{
	struct kmem_cache *c;
	void *objs[64];
	int ret;

	c = kmem_cache_create("reap_test", 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create reap test cache");

	/* Grow by allocating one object */
	void *first = kmem_cache_alloc(c, KM_SLEEP);
	TEST_ASSERT(first != 0, "grow first slab");

	/* Reap should fail  - slab is not empty */
	ret = kmem_cache_reap(c);
	TEST_ASSERT(ret != 0, "reap fails on non-empty slab");

	kmem_cache_free(c, first);

	/* Now reap should succeed */
	ret = kmem_cache_reap(c);
	TEST_ASSERT(ret == 0, "reap succeeds on empty slab");

	kmem_cache_destroy(c);

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_reap reap multiple slabs                               */
/* -------------------------------------------------------------------------- */
static int test_slab_reap_multi(void)
{
	struct kmem_cache *c;
	void *objs[128];
	int n = 0;
	int per_slab;

	c = kmem_cache_create("reap_multi", 128, 0, 0, 0);
	TEST_ASSERT(c != 0, "create multi-reap cache");

	/* Grow two slabs explicitly -don't depend on kalloc via implicit grow
	 */
	TEST_ASSERT(kmem_cache_grow(c) == PGSIZE, "grow first slab");
	TEST_ASSERT(kmem_cache_grow(c) == PGSIZE, "grow second slab");

	per_slab = (PGSIZE - sizeof(struct kmem_slab)) / (int) c->obj_size;
	TEST_ASSERT(per_slab > 0, "valid per-slab capacity");

	/* Fill both slabs */
	for (int i = 0; i < per_slab * 2; i++) {
		void *obj = kmem_cache_alloc(c, KM_NOSLEEP);
		if (!obj)
			break;
		objs[n++] = obj;
	}
	TEST_ASSERT(n >= per_slab * 2, "filled two slabs");

	/* Free everything */
	for (int i = 0; i < n; i++)
		kmem_cache_free(c, objs[i]);

	/* Reap one slab at a time */
	int reap_count = 0;
	while (kmem_cache_reap(c) == 0)
		reap_count++;
	TEST_ASSERT(reap_count > 0, "reaped at least one slab");

	kmem_cache_destroy(c);

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_destroy basic                                          */
/* -------------------------------------------------------------------------- */
static int test_slab_destroy_basic(void)
{
	struct kmem_cache *c;

	c = kmem_cache_create("destroy_me", 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create cache for destroy");

	void *obj = kmem_cache_alloc(c, KM_SLEEP);
	TEST_ASSERT(obj != 0, "alloc before destroy");
	kmem_cache_free(c, obj);

	kmem_cache_destroy(c);
	TEST_ASSERT(1, "destroy succeeded");

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: kmem_cache_destroy  - destroy fails if objects still in use */
/* -------------------------------------------------------------------------- */
static int test_slab_destroy_with_objects(void)
{
	struct kmem_cache *c;

	c = kmem_cache_create("destroy_busy", 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create busy cache");

	void *obj = kmem_cache_alloc(c, KM_SLEEP);
	TEST_ASSERT(obj != 0, "alloc object");

	/* destroy with outstanding objects  - should log warning, not crash */
	kmem_cache_destroy(c);
	TEST_ASSERT(1, "destroy with busy objects does not crash");

	/* Cleanup */ /* Note: can't free since cache is partially destroyed */
	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: multiple independent caches                                         */
/* -------------------------------------------------------------------------- */
static int test_slab_multi_cache(void)
{
	void *a;
	void *b;
	void *c;
	struct kmem_cache *ca;
	struct kmem_cache *cb;
	struct kmem_cache *cc;

	ca = kmem_cache_create("cache_A", 32, 0, 0, 0);
	cb = kmem_cache_create("cache_B", 64, 0, 0, 0);
	cc = kmem_cache_create("cache_C", 128, 0, 0, 0);

	TEST_ASSERT(ca && cb && cc, "three caches created");

	a = kmem_cache_alloc(ca, KM_SLEEP);
	b = kmem_cache_alloc(cb, KM_SLEEP);
	c = kmem_cache_alloc(cc, KM_SLEEP);

	TEST_ASSERT(a && b && c, "alloc from three caches");
	TEST_ASSERT(a != b && b != c && a != c,
		    "objects from different caches are distinct");
	TEST_ASSERT(ca->obj_size == 32, "cache A size correct");
	TEST_ASSERT(cb->obj_size == 64, "cache B size correct");
	TEST_ASSERT(cc->obj_size == 128, "cache C size correct");

	kmem_cache_free(ca, a);
	kmem_cache_free(cb, b);
	kmem_cache_free(cc, c);

	kmem_cache_destroy(ca);
	kmem_cache_destroy(cb);
	kmem_cache_destroy(cc);

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: grow explicit                                                       */
/* -------------------------------------------------------------------------- */
static int test_slab_grow(void)
{
	struct kmem_cache *c;
	int ret;

	c = kmem_cache_create("grow_test", 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create grow test cache");

	uint64 size_before = c->total_size;

	ret = kmem_cache_grow(c);
	TEST_ASSERT(ret == PGSIZE, "grow returns PGSIZE");
	TEST_ASSERT(c->total_size == size_before + PGSIZE,
		    "total_size incremented");

	/* Should be able to alloc KM_NOSLEEP now */
	void *obj = kmem_cache_alloc(c, KM_NOSLEEP);
	TEST_ASSERT(obj != 0, "KM_NOSLEEP succeeds after grow");
	kmem_cache_free(c, obj);

	ret = kmem_cache_reap(c);
	TEST_ASSERT(ret == 0, "reap after grow");

	kmem_cache_destroy(c);

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: slab NULL safety                                                    */
/* -------------------------------------------------------------------------- */
static int test_slab_null_safety(void)
{
	kmem_cache_free(0, (void *) 0x1000);
	TEST_ASSERT(1, "kmem_cache_free(NULL, ...) does not crash");

	int ret = kmem_cache_reap(0);
	TEST_ASSERT(ret != 0, "kmem_cache_reap(NULL) returns error");

	void *obj = kmem_cache_alloc(0, KM_SLEEP);
	TEST_ASSERT(obj == 0, "kmem_cache_alloc(NULL, ...) returns NULL");

	kmem_cache_destroy(0);
	TEST_ASSERT(1, "kmem_cache_destroy(NULL) does not crash");

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: slab state transitions                                              */
/* -------------------------------------------------------------------------- */
static int test_slab_state_transitions(void)
{
	struct kmem_cache *c;
	void *objs[128];
	int n;
	int total_in_slab;

	c = kmem_cache_create("state_test", 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create state test cache");

	/* Allocate one to trigger grow; determine per-slab capacity */
	void *first = kmem_cache_alloc(c, KM_SLEEP);
	total_in_slab = (PGSIZE - sizeof(struct kmem_slab)) / (int) c->obj_size;
	TEST_ASSERT(total_in_slab > 0, "valid per-slab count");

	/* Allocate remaining objects in that slab */
	objs[0] = first;
	for (n = 1; n < total_in_slab; n++) {
		objs[n] = kmem_cache_alloc(c, KM_SLEEP);
		if (!objs[n])
			break;
	}

	/* Free half  - slab is partial */
	int half = n / 2;
	for (int i = 0; i < half; i++)
		kmem_cache_free(c, objs[i]);

	/* Re-alloc  - should come from partial slab */
	void *r = kmem_cache_alloc(c, KM_SLEEP);
	TEST_ASSERT(r != 0, "re-alloc from partial slab");
	kmem_cache_free(c, r);

	/* Free the rest  - slab becomes empty */
	for (int i = half; i < n; i++)
		kmem_cache_free(c, objs[i]);

	/* Alloc again  - should come from empty slab */
	void *again = kmem_cache_alloc(c, KM_NOSLEEP);
	TEST_ASSERT(again != 0, "alloc from empty slab");
	kmem_cache_free(c, again);

	kmem_cache_destroy(c);

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: large object cache (near page size)                                 */
/* -------------------------------------------------------------------------- */
static int test_slab_large_object(void)
{
	struct kmem_cache *c;
	void *objs[4];
	int n = 0;

	c = kmem_cache_create("large", 2000, 0, 0, 0);
	TEST_ASSERT(c != 0, "create large object cache");

	for (int i = 0; i < 4; i++) {
		objs[i] = kmem_cache_alloc(c, KM_SLEEP);
		if (!objs[i])
			break;
		n++;
	}

	/* With 2000-byte objects, a 4096 page can hold at most 1-2 */
	TEST_ASSERT(n >= 1, "large object allocation works");

	for (int i = 0; i < n; i++)
		kmem_cache_free(c, objs[i]);

	kmem_cache_destroy(c);

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: very small objects (<= 8 bytes)                                     */
/* -------------------------------------------------------------------------- */
static int test_slab_small_object(void)
{
	struct kmem_cache *c;
	void *objs[256];
	int n;

	c = kmem_cache_create("small", 1, 0, 0, 0);
	TEST_ASSERT(c != 0, "create small object cache");
	TEST_ASSERT(c->obj_size == 8, "min size promoted to 8");

	/* Allocate many */
	for (n = 0; n < 256; n++) {
		objs[n] = kmem_cache_alloc(c, KM_SLEEP);
		if (!objs[n])
			break;
	}
	/* With 8-byte objects, one 4K page holds ~500+ */
	TEST_ASSERT(n >= 100, "8-byte objects: at least 100 fit in one slab");
	TEST_ASSERT(is_aligned(objs[0], 8), "8-byte objects are aligned");

	for (int i = 1; i < n; i++)
		kmem_cache_free(c, objs[i]);

	kmem_cache_destroy(c);

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: cache name round-trip                                               */
/* -------------------------------------------------------------------------- */
static int test_slab_name(void)
{
	struct kmem_cache *c;

	c = kmem_cache_create("hello_world", 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create named cache");

	/* Name starts with "hello_world" */
	TEST_ASSERT(c->name[0] == 'h', "name char 0");
	TEST_ASSERT(c->name[1] == 'e', "name char 1");
	TEST_ASSERT(c->name[5] == '_', "name char 5");

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: freelist ordering (LIFO behavior)                                   */
/* -------------------------------------------------------------------------- */
static int test_slab_freelist_lifo(void)
{
	struct kmem_cache *c;
	void *a;
	void *b;
	void *c1;
	void *c2;

	c = kmem_cache_create("lifo", 128, 0, 0, 0);

	a = kmem_cache_alloc(c, KM_SLEEP);
	b = kmem_cache_alloc(c, KM_SLEEP);

	kmem_cache_free(c, b);
	kmem_cache_free(c, a);

	/* LIFO: last freed = first allocated */
	c1 = kmem_cache_alloc(c, KM_NOSLEEP);
	c2 = kmem_cache_alloc(c, KM_NOSLEEP);

	TEST_ASSERT(c1 == a, "first re-alloc is last freed (LIFO)");
	TEST_ASSERT(c2 == b, "second re-alloc is first freed (LIFO)");

	kmem_cache_free(c, c1);
	kmem_cache_free(c, c2);

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: total_size tracking                                                 */
/* -------------------------------------------------------------------------- */
static int test_slab_total_size(void)
{
	struct kmem_cache *c;

	c = kmem_cache_create("size_track", 256, 0, 0, 0);
	TEST_ASSERT(c != 0, "create size tracking cache");
	TEST_ASSERT(c->total_size == 0,
		    "total_size starts at 0 (no slabs yet)");

	void *obj = kmem_cache_alloc(c, KM_SLEEP);
	TEST_ASSERT(c->total_size == PGSIZE,
		    "total_size == PGSIZE after first alloc");

	kmem_cache_grow(c);
	TEST_ASSERT(c->total_size == (uint64) 2 * PGSIZE,
		    "total_size after explicit grow");

	kmem_cache_free(c, obj);

	kmem_cache_reap(c);
	TEST_ASSERT(c->total_size <= PGSIZE, "total_size decreases after reap");

	kmem_cache_destroy(c);

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: random stress - alloc/free in random order */
/* -------------------------------------------------------------------------- */
static int test_slab_random_stress(void)
{
	struct kmem_cache *c;
	void *objs[64];
	int allocated[64] = {0};
	int alive = 0;

	c = kmem_cache_create("random", 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create random stress cache");

	/* Fill one slab */
	int per_slab = (PGSIZE - sizeof(struct kmem_slab)) / (int) c->obj_size;
	for (int i = 0; i < per_slab; i++) {
		objs[i] = kmem_cache_alloc(c, KM_SLEEP);
		TEST_ASSERT(objs[i] != 0, "random prefill");
		*(uint64 *) objs[i] = (uint64) i;
		allocated[i] = 1;
		alive++;
	}

	/* Random alloc/free cycle */
	for (int round = 0; round < 20; round++) {
		for (int i = 0; i < per_slab; i++) {
			if (allocated[i]) {
				/* Verify pattern */
				TEST_ASSERT(*(uint64 *) objs[i] == (uint64) i,
					    "random: no corruption");
			}
		}
		/* Free half, alloc new ones */
		for (int i = 0; i < per_slab; i++) {
			if (allocated[i] && (i % 3) == 0) {
				kmem_cache_free(c, objs[i]);
				allocated[i] = 0;
				alive--;
			}
		}
		for (int i = 0; i < per_slab && alive < per_slab; i++) {
			if (!allocated[i]) {
				objs[i] = kmem_cache_alloc(c, KM_SLEEP);
				TEST_ASSERT(objs[i] != 0, "random re-alloc");
				*(uint64 *) objs[i] = (uint64) i;
				allocated[i] = 1;
				alive++;
			}
		}
	}

	/* Free everything */
	for (int i = 0; i < per_slab; i++) {
		if (allocated[i])
			kmem_cache_free(c, objs[i]);
	}

	kmem_cache_destroy(c);
	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: double-free detection - free_objs should not exceed total_objs      */
/* -------------------------------------------------------------------------- */
static int test_slab_double_free(void)
{
	struct kmem_cache *c;
	void *a;
	void *b;

	c = kmem_cache_create("double_free", 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create double-free cache");

	a = kmem_cache_alloc(c, KM_SLEEP);
	b = kmem_cache_alloc(c, KM_SLEEP);
	TEST_ASSERT(a != 0 && b != 0, "alloc two objects");
	TEST_ASSERT(a != b, "objects are distinct");

	kmem_cache_free(c, a);
	kmem_cache_free(c, b);

	/* Double-free b - slab should detect this and reject it.
	 * The freelist must remain intact: a then b (normal LIFO order). */
	kmem_cache_free(c, b);

	/* Allocate back - normal LIFO: last freed is b, then a */
	void *x = kmem_cache_alloc(c, KM_NOSLEEP);
	void *y = kmem_cache_alloc(c, KM_NOSLEEP);
	void *z = kmem_cache_alloc(c, KM_NOSLEEP);
	TEST_ASSERT(x == b, "first re-alloc: last freed is b");
	TEST_ASSERT(y == a, "second re-alloc: a (double-free was blocked)");
	TEST_ASSERT(z != a && z != b && z != x && z != y,
		    "third re-alloc is distinct (no freelist cycle)");

	kmem_cache_free(c, x);
	kmem_cache_free(c, y);
	kmem_cache_free(c, z);

	kmem_cache_destroy(c);
	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: boundary overwrite - verify adjacent object isolation               */
/* -------------------------------------------------------------------------- */
static int test_slab_boundary_overwrite(void)
{
	struct kmem_cache *c;
	void *objs[4];

	c = kmem_cache_create("boundary", 128, 0, 0, 0);
	TEST_ASSERT(c != 0, "create boundary test cache");

	/* Alloc 3 objects; obj1 sits between obj0 and obj2 */
	objs[0] = kmem_cache_alloc(c, KM_SLEEP);
	objs[1] = kmem_cache_alloc(c, KM_SLEEP);
	objs[2] = kmem_cache_alloc(c, KM_SLEEP);
	TEST_ASSERT(objs[0] && objs[1] && objs[2], "alloc three objects");

	/* Write a marker at start of obj0 and obj2 */
	*(uint64 *) objs[0] = 0xDEAD0000DEAD0001ULL;
	*(uint64 *) objs[2] = 0xDEAD0000DEAD0003ULL;

	/* Overwrite obj1 entirely - must not touch obj0 or obj2 */
	memset(objs[1], 0xFF, 128);

	TEST_ASSERT(*(uint64 *) objs[0] == 0xDEAD0000DEAD0001ULL,
		    "obj0 not corrupted by obj1 overwrite");
	TEST_ASSERT(*(uint64 *) objs[2] == 0xDEAD0000DEAD0003ULL,
		    "obj2 not corrupted by obj1 overwrite");

	/* Write exactly at obj1 boundary - should not leak into neighbor */
	for (int i = 0; i < 128; i++)
		((char *) objs[1])[i] = (char) i;

	TEST_ASSERT(*(uint64 *) objs[0] == 0xDEAD0000DEAD0001ULL,
		    "obj0 not corrupted by boundary fill");
	TEST_ASSERT(*(uint64 *) objs[2] == 0xDEAD0000DEAD0003ULL,
		    "obj2 not corrupted by boundary fill");

	kmem_cache_free(c, objs[0]);
	kmem_cache_free(c, objs[1]);
	kmem_cache_free(c, objs[2]);

	kmem_cache_destroy(c);
	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: slab metadata consistency - verify free_objs and list integrity     */
/* -------------------------------------------------------------------------- */
static int test_slab_metadata_consistency(void)
{
	struct kmem_cache *c;
	void *objs[128];
	int n;

	c = kmem_cache_create("meta", 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create metadata test cache");

	int per_slab = (PGSIZE - sizeof(struct kmem_slab)) / (int) c->obj_size;

	/* Phase 1: fill one slab */
	for (n = 0; n < per_slab; n++) {
		objs[n] = kmem_cache_alloc(c, KM_SLEEP);
		if (!objs[n])
			break;
	}
	TEST_ASSERT(n == per_slab, "filled one slab");

	/* Phase 2: verify slab is in full list */
	kmem_cache_grow(c); /* add one more empty slab */

	/* Alloc from the new empty slab */
	void *extra = kmem_cache_alloc(c, KM_NOSLEEP);
	TEST_ASSERT(extra != 0, "alloc from second slab");

	/* Free one from first slab - moves it from full to partial */
	kmem_cache_free(c, objs[0]);

	/* Re-alloc succeeds (may come from slab2's partial list, not slab1) */
	void *r = kmem_cache_alloc(c, KM_NOSLEEP);
	TEST_ASSERT(r != 0, "re-alloc after freeing one object");

	/* Phase 3: free all objects */
	kmem_cache_free(c, r); /* free the re-allocated one */
	for (int i = 1; i < n; i++)
		kmem_cache_free(c, objs[i]);
	kmem_cache_free(c, extra);

	/* Phase 4: after freeing all, all slabs should be reapable */
	int reaped = 0;
	while (kmem_cache_reap(c) == 0)
		reaped++;
	TEST_ASSERT(reaped >= 2, "reaped all slabs");

	kmem_cache_destroy(c);
	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Test: OOM handling - graceful behavior under memory pressure              */
/* -------------------------------------------------------------------------- */
static int test_slab_oom_handling(void)
{
	struct kmem_cache *c;

	/* 1. KM_NOSLEEP on empty cache returns NULL */
	c = kmem_cache_create("oom", 64, 0, 0, 0);
	TEST_ASSERT(c != 0, "create OOM test cache");

	void *obj = kmem_cache_alloc(c, KM_NOSLEEP);
	TEST_ASSERT(obj == 0, "KM_NOSLEEP on empty cache returns NULL");

	/* 2. After grow, KM_NOSLEEP succeeds */
	TEST_ASSERT(kmem_cache_grow(c) == PGSIZE, "grow for OOM test");
	obj = kmem_cache_alloc(c, KM_NOSLEEP);
	TEST_ASSERT(obj != 0, "KM_NOSLEEP succeeds after grow");
	kmem_cache_free(c, obj);

	/* 3. reap on a cache with only a full slab returns error */
	int per_slab = (PGSIZE - sizeof(struct kmem_slab)) / (int) c->obj_size;
	void *objs[64];
	for (int i = 0; i < per_slab; i++)
		objs[i] = kmem_cache_alloc(c, KM_NOSLEEP);

	int ret = kmem_cache_reap(c);
	TEST_ASSERT(ret != 0, "reap fails when slab is full");

	for (int i = 0; i < per_slab; i++)
		kmem_cache_free(c, objs[i]);

	/* 4. reap on empty cache */
	ret = kmem_cache_reap(c);
	TEST_ASSERT(ret == 0, "reap succeeds after freeing all");

	/* 5. second reap on same empty cache fails */
	ret = kmem_cache_reap(c);
	TEST_ASSERT(ret != 0, "second reap fails (no slabs left)");

	kmem_cache_destroy(c);

	/* 6. destroy null cache is safe */
	kmem_cache_destroy(0);

	/* 7. alloc from null cache returns NULL */
	TEST_ASSERT(kmem_cache_alloc(0, KM_SLEEP) == 0,
		    "KM_SLEEP from NULL cache returns NULL");

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  Run all tests                                                             */
/* -------------------------------------------------------------------------- */
void test_slab_run_all(void)
{
	LOG_BANNER("=== slab allocator tests ===");

	LOG_PHASE("init");
	RUN_TEST(test_slab_init);

	LOG_PHASE("create");
	RUN_TEST(test_slab_create);
	RUN_TEST(test_slab_create_bad_align);
	RUN_TEST(test_slab_create_align);
	RUN_TEST(test_slab_create_ctor_dtor);
	RUN_TEST(test_slab_create_name);

	LOG_PHASE("alloc");
	RUN_TEST(test_slab_alloc_single);
	RUN_TEST(test_slab_alloc_fill_slab);
	RUN_TEST(test_slab_alloc_nosleep_empty);
	RUN_TEST(test_slab_alloc_alignment);
	RUN_TEST(test_slab_alloc_default_alignment);
	RUN_TEST(test_slab_alloc_reuse);
	RUN_TEST(test_slab_alloc_stress);
	RUN_TEST(test_slab_small_object);
	RUN_TEST(test_slab_large_object);

	LOG_PHASE("ctor/dtor");
	RUN_TEST(test_slab_ctor_dtor);

	LOG_PHASE("reap");
	RUN_TEST(test_slab_reap_basic);
	RUN_TEST(test_slab_reap_multi);

	LOG_PHASE("destroy");
	RUN_TEST(test_slab_destroy_basic);
	RUN_TEST(test_slab_destroy_with_objects);

	LOG_PHASE("misc");
	RUN_TEST(test_slab_multi_cache);
	RUN_TEST(test_slab_grow);
	RUN_TEST(test_slab_null_safety);
	RUN_TEST(test_slab_state_transitions);
	RUN_TEST(test_slab_freelist_lifo);
	RUN_TEST(test_slab_name);
	RUN_TEST(test_slab_total_size);

	LOG_PHASE("edge cases");
	RUN_TEST(test_slab_random_stress);
	RUN_TEST(test_slab_double_free);
	RUN_TEST(test_slab_boundary_overwrite);
	RUN_TEST(test_slab_metadata_consistency);
	RUN_TEST(test_slab_oom_handling);

	LOG_BANNER("=== all slab tests passed ===");
}

/* -------------------------------------------------------------------------- */
/*  Entry point called from kernel init                                       */
/* -------------------------------------------------------------------------- */
void test_slab(void)
{
	test_slab_run_all();
}
