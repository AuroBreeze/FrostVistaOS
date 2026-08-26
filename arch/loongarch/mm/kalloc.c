#include "kernel/mm/kalloc.h"
#include "asm/mm.h"
#include "platform/uart.h"
#include "kernel/types.h"
#include "kernel/string.h"

// Initialization
struct freeMemory FMM;
struct IdleMM head;
int cnt = 0;
char *ekalloc_ptr = (char *) _kernel_end;

static void freerange(void *pa_start, void *pa_end);

void kfree(void *va);

// Enable sv39 paging and high address mapping
void kalloc_init()
{
	kprintf("kalloc_init start\n");
	// initlock(&mem_lock, "&mem_lock");
	FMM.freelist = &head;
	head.next = FMM.freelist;
	freerange((void *) ((uint64) ekalloc_ptr), (void *) PHYSTOP_HIGH);

	kprintf("Total Memory Pages: %d\n", FMM.size);
	kprintf("kalloc_init end\n");
}

// enable sv39 paging and high address mapping
// Mount all released memory to the virtual high address range
static void freerange(void *pa_start, void *pa_end)
{
	kprintf("freerange: %p - %p\n", pa_start, pa_end);
	if (!IS_RAM_KVA((uint64) pa_start) || !IS_RAM_KVA((uint64) pa_end)) {
		kprintf("pa: %p\npe: %p\n", pa_start, pa_end);
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

	if (!IS_RAM_KVA(p)) {
		kprintf("va: %p\n", p);
		panic("kfree: Low-address space cannot be released");
	}

	if ((p % PGSIZE != 0) || (p > PHYSTOP_HIGH) ||
	    (p < (uint64) _kernel_end)) {
		// kprintf("kfree: _kernel_end: %d\n", _kernel_end);
		kprintf("PHYSTOP: %p\n", (uint64) PHYSTOP_LOW);
		kprintf("_kernel_end: %p\n", (uint64) _kernel_end);
		kprintf("align: %x   _kernel_end: %x   PHYSTOP: %x\n",
			p % PGSIZE != 0,
			p<(uint64) _kernel_end, p> PHYSTOP_HIGH);
		kprintf("va: %p  p: %p\n", (void *) va, (void *) p);

		panic("kfree encounter an error");
	}

	struct IdleMM *M;

	// kprintf("kva: %p\n", (void *)kva);
	memset((void *) kva, 0, PGSIZE);

	M = (struct IdleMM *) kva;
	M->next = head.next;
	head.next = M;
	FMM.size++;
}

// Enable sv39 paging and high address mapping
// return to high address
void *kalloc()
{
	if (head.next == &head) {
		kprintf("kalloc memory is not enough\n");
		return 0;
	}

	struct IdleMM *temp;

	temp = head.next;
	head.next = temp->next;
	FMM.size--;

	memset(temp, 0, PGSIZE);
	return (void *) temp;
}

void *ekalloc(void)
{
	if (((uint64) ekalloc_ptr % PGSIZE) != 0)
		panic("ekalloc panic");

	void *ret = ekalloc_ptr;
	// kprintf("ekalloc: %p\n", (void *)ret);
	ekalloc_ptr += PGSIZE;
	return (void *) VA2PA((uint64) ret);
}
