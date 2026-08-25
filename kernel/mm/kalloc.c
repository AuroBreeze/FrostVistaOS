#define LOG_MODULE " MEM"

#include "kernel/mm/kalloc.h"
#include "kernel/arch/mm.h"
#include "kernel/defs.h"
#include "kernel/string.h"
#include "kernel/log.h"
#include "kernel/spinlock.h"
#include "kernel/types.h"

// Initialization
struct freeMemory FMM;
struct IdleMM head;
int cnt = 0;
char *ekalloc_ptr = (char *) _kernel_end;
struct spinlock mem_lock;

int refcnt[DRAM_SIZE / PGSIZE] = {0};

/**
 * refcnt_add - add the reference count of a page
 * @va: the virtual address
 *
 * Lock Contract:
 *  Entry: will acquire the mem_lock
 *  Exit: will release the mem_lock
 *
 * Return: 0
 * */
int refcnt_inc(uint64 va)
{
	acquire(&mem_lock);
	int refnum = (int64) (arch_kva_to_pa(va) - DRAM_BASE_LOW) / PGSIZE;
	if (refcnt[refnum] <= 0) {
		panic("refcnt_inc: refcnt is 0");
	}
	refcnt[refnum]++;
	release(&mem_lock);
	return 0;
}

int refcnt_dec(uint64 va)
{
	acquire(&mem_lock);
	int refnum = (int64) (arch_kva_to_pa(va) - DRAM_BASE_LOW) / PGSIZE;
	if (refcnt[refnum] <= 0) {
		panic("refcnt_dec: refcnt is 0");
	}
	refcnt[refnum]--;
	release(&mem_lock);
	return 0;
}

static void freerange(void *pa_start, void *pa_end);

// Enable sv39 paging and high address mapping
void kalloc_init()
{
	LOG_INFO("kalloc_init start");
	initlock(&mem_lock, "&mem_lock");
	FMM.freelist = &head;
	head.next = FMM.freelist;
	freerange((void *) ((uint64) ekalloc_ptr), (void *) PHYSTOP_HIGH);

	LOG_INFO("Total Memory Pages: %d", FMM.size);
	LOG_INFO("kalloc_init end");
}

// enable sv39 paging and high address mapping
// Mount all released memory to the virtual high address range
static void freerange(void *pa_start, void *pa_end)
{
	LOG_TRACE("freerange: %p - %p", pa_start, pa_end);
	if (!arch_is_ram_kva((uint64) pa_start) ||
	    !arch_is_ram_kva((uint64) pa_end)) {
		LOG_ERROR("pa: %p\npe: %p\n", pa_start, pa_end);
		panic("freerange: It must be a high address");
	}
	char *ps = (char *) pa_start;
	char *pe = (char *) pa_end;
	for (; ps + PGSIZE <= pe; ps += PGSIZE) {
		kfree((void *) ps);
	}
}

// Enable sv39 paging and high address mapping
// pa must be a high address
// Mount virtual high addresses after releasing memory
/**
 * kfree - Release a page
 * @va: the virtual address to free
 *
 * Context:
 *
 * Return: void
 */
// NOTE:  I have changed the PA here to VA, because it was previously written as
// PA due to using an identity mapping and not understanding address
// translation. Now it should be changed to VA to clarify the parameters that
// need to be filled in. There is no distinction between high and low for PA.
void kfree(void *va)
{
	uint64 p = (uint64) va;
	uint64 kva = (uint64) va;

	if (!arch_is_ram_kva(p)) {
		LOG_ERROR("va: %p", p);
		panic("kfree: Low-address space cannot be released");
	}

	if ((p % PGSIZE != 0) || (p > PHYSTOP_HIGH) ||
	    (p < (uint64) _kernel_end)) {
		// LOG_DEBUG("kfree: _kernel_end: %d\n", _kernel_end);
		LOG_TRACE("PHYSTOP: %p", (uint64) PHYSTOP_LOW);
		LOG_TRACE("_kernel_end: %p", (uint64) _kernel_end);
		LOG_TRACE("align: %x   _kernel_end: %x   PHYSTOP: %x",
			  p % PGSIZE != 0,
			  p<(uint64) _kernel_end, p> PHYSTOP_HIGH);
		LOG_TRACE("va: %p  p: %p", (void *) va, (void *) p);

		panic("kfree encounter an error");
	}

	acquire(&mem_lock);
	if (refcnt[(int64) (arch_kva_to_pa(p) - DRAM_BASE_LOW) / PGSIZE] > 1) {
		refcnt[(int64) (arch_kva_to_pa(p) - DRAM_BASE_LOW) / PGSIZE]--;
		release(&mem_lock);
		return;
	}
	release(&mem_lock);

	struct IdleMM *M;

	// kprintf("kva: %p\n", (void *)kva);
	memset((void *) kva, 0, PGSIZE);

	acquire(&mem_lock);
	M = (struct IdleMM *) kva;
	M->next = head.next;
	head.next = M;
	FMM.size++;
	release(&mem_lock);
}

// Enable sv39 paging and high address mapping
// return to high address
void *kalloc()
{
	acquire(&mem_lock);
	if (head.next == &head) {
		release(&mem_lock);
		LOG_WARN("kalloc memory is not enough");
		return 0;
	}

	struct IdleMM *temp;

	temp = head.next;
	head.next = temp->next;
	FMM.size--;

	int refnum =
	    (int64) (arch_kva_to_pa((uint64) temp) - DRAM_BASE_LOW) / PGSIZE;
	refcnt[refnum] = 1;

	release(&mem_lock);
	memset(temp, 0, PGSIZE);
	return (void *) temp;
}

void *ekalloc(void)
{
	if (((uint64) ekalloc_ptr % PGSIZE) != 0)
		panic("ekalloc panic");

	void *ret = ekalloc_ptr;
	// LOG_TRACE("ekalloc: %p", (void *)ret);
	ekalloc_ptr += PGSIZE;
	return (void *) arch_kva_to_pa((uint64) ret);
}
