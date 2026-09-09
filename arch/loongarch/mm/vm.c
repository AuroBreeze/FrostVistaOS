#define LOG_MODULE "MM"

#include "asm/mm.h"
#include "asm/loongarch.h"
#include "asm/vm.h"
#include "kernel/arch/mm.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "platform/uart.h"
#include "platform/power.h"
#include "kernel/string.h"
#include "kernel/types.h"
#include "kernel/proc.h"

extern pagetable_t kernel_pgdl;
extern pagetable_t kernel_pgdh;

/*
 * LoongArch selects the low and high virtual address spaces through two
 * independent page-table root CSRs.  User mappings belong to PGDL, while
 * the kernel mappings remain in the boot-created PGDH root.
 */
void switch_to_process(pagetable_t pagetable)
{
	if (pagetable == 0)
		panic("switch_to_process: null page table");

	w_pgdl(arch_kva_to_pa((uint64) pagetable));
	w_pgdh(DMW0_VA2PA((uint64) kernel_pgdh));
	sfence_vma();
}

void switch_to_kernel(void)
{
	if (kernel_pgdl == 0 || kernel_pgdh == 0)
		panic("switch_to_kernel: kernel page table is not initialized");

	w_pgdl(DMW0_VA2PA((uint64) kernel_pgdl));
	w_pgdh(DMW0_VA2PA((uint64) kernel_pgdh));
	sfence_vma();
}

void device_mapping()
{
	if (kvmmap_mmio_current(UART_PAGE_VA, UART_PAGE_PA, PGSIZE,
				LA_PTE_PLV0 | LA_PTE_W | LA_PTE_NX |
				    LA_PTE_MAT_SUC) < 0) {
		panic("device_mapping: map UART failed");
	}
	if (kvmmap_mmio_current(GED_POWER_PAGE_VA, GED_POWER_PAGE_PA, PGSIZE,
				LA_PTE_PLV0 | LA_PTE_W | LA_PTE_NX |
				    LA_PTE_MAT_SUC) < 0) {
		panic("device_mapping: map poweroff failed");
	}

	/* 丢弃可能存在的旧项，并先通过正式高半区地址验证 UART。 */
	invtlb_all();
	uart_use_mapped_io();
	LOG_INFO("UART high-half mapping enabled");

	/* UART 已不再依赖 DMW1，清除全部 PLV 使能位以关闭该窗口。 */
	w_dmw1(0);
	asm volatile("dbar 0\n\tibar 0" ::: "memory");
	LOG_INFO("DMW1 disabled");
}

/*
 * 在指定页表中查找虚拟地址对应的最终页表项。
 *
 * level=2：PGDL 下的 Dir2
 * level=1：Dir2 下的 Dir1
 * level=0：Dir1 下的最终页表 PT
 *
 * 调用者负责传入与 va 对应的根页表。中间目录项不存在时，alloc 非零
 * 表示分配并清零新的页表页。目录项中的页表地址必须写入物理地址，
 * 访问页表内容时则使用正式高半区直接映射。
 */
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc)
{
	if (pagetable == 0) {
		return 0;
	}

	for (int level = 2; level > 0; level--) {
		pte_t *pte = &pagetable[loongarch_vpn(va, level)];
		if (LA_PTE_IS_VALID(*pte)) {
			uint64 child_pa = LA_PTE_PA(*pte);
			pagetable = (pagetable_t) KERNEL_PA2VA(child_pa);
			continue;
		}

		if (!alloc) {
			return 0;
		}

		pagetable_t child = (pagetable_t) kalloc();
		if (child == 0) {
			return 0;
		}

		/* kalloc() 返回正式高半区地址，并已清零整页。 */
		/* 非大页目录项只保存下一级页表的物理地址。 */
		*pte = LA_PA_PTE(KERNEL_VA2PA((uint64) child)) | LA_PTE_V |
		       LA_PTE_P;
		pagetable = child;
	}

	return &pagetable[loongarch_vpn(va, 0)];
}

/*
 * 在当前活动地址空间中查找页表项。PGDL/PGDH CSR 保存物理根地址，
 * 这里通过 DMW0 读取启动页表根；其下新建的页表页由 walk() 使用正式
 * 高半区直接映射访问。
 */
pte_t *walk_current(uint64 va, int alloc)
{
	uint64 root_pa;

	if (loongarch_is_high_va(va)) {
		root_pa = r_pgdh();
	} else if (loongarch_is_low_va(va)) {
		root_pa = r_pgdl();
	} else {
		return 0;
	}

	if (root_pa == 0)
		return 0;

	return walk((pagetable_t) DMW0_PA2VA(root_pa), va, alloc);
}

int mappages(pagetable_t pagetable, uint64 va, uint64 pa, uint64 size,
	     uint64 perm)
{
	if (size == 0 || va % PGSIZE != 0 || pa % PGSIZE != 0 ||
	    size % PGSIZE != 0)
		return -1;

	/* 防止计算映射末尾地址时发生无符号整数溢出。 */
	if (va + size < va || pa + size < pa)
		return -1;

	uint64 a;
	uint64 last;
	pte_t *pte;

	a = va;
	last = va + size - PGSIZE;

	for (;;) {
		if ((pte = walk(pagetable, a, 1)) == 0) {
			return -1;
		}

		if (LA_PTE_IS_VALID(*pte)) {
			panic("mappages: remap");
		}

		/* 可写页必须同时具备 PTE.W 和 PTE.D，TLB 才允许写访问。 */
		if (perm & LA_PTE_W)
			perm |= LA_PTE_D;
		*pte =
		    LA_PA_PTE(pa) | perm | LA_PTE_V | LA_PTE_P | LA_PTE_MAT_CC;
		if (a == last) {
			break;
		}

		a += PGSIZE;
		pa += PGSIZE;
	}
	return 0;
}

uint64 walk_addr(pagetable_t pagetable, uint64 va)
{
	// WARNING: Pay attention to the range of VA addresses
	pte_t *pte = walk(pagetable, va, 0);
	if (pte == 0)
		return 0;
	if (!LA_PTE_IS_VALID(*pte)) {
		return 0;
	}

	uint64 pa;
	pa = LA_PTE_PA(*pte);
	return pa;
}

/**
 * kvmmap - Map physical memory to virtual memory
 * @pagetable : Base address of the target pagetable
 * @va : Virtual address
 * @pa : Physical address
 * @size : Memory size
 * @perm : Permission
 *
 * Context: Map physical memory to virtual memory
 *
 * Return: 0 on success, -1 on error
 */
int kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 size,
	   uint64 perm)
{
	return mappages(pagetable, va, pa, size, pte_from_perm(perm));
}

/*
 * 在当前活动页表中建立 MMIO 映射。
 *
 * 与 mappages() 不同，此函数不会默认附加 LA_PTE_MAT_CC；调用者必须
 * 明确指定设备所需的内存访问类型，通常为 LA_PTE_MAT_SUC。这样可以
 * 防止把寄存器页错误地映射为可缓存普通内存。
 */
int kvmmap_mmio_current(uint64 va, uint64 pa, uint64 size, uint64 perm)
{
	if (size == 0 || va % PGSIZE != 0 || pa % PGSIZE != 0 ||
	    size % PGSIZE != 0) {
		return -1;
	}
	if (va + size < va || pa + size < pa) {
		return -1;
	}

	uint64 last = va + size - PGSIZE;
	for (;;) {
		pte_t *pte = walk_current(va, 1);
		if (pte == 0 || LA_PTE_IS_VALID(*pte)) {
			return -1;
		}

		/* 可写 MMIO 页同样需要置 D，才能通过 TLB 的写权限检查。 */
		uint64 flags = perm;
		if (flags & LA_PTE_W) {
			flags |= LA_PTE_D;
		}
		*pte = LA_PA_PTE(pa) | flags | LA_PTE_V | LA_PTE_P;

		if (va == last) {
			break;
		}
		va += PGSIZE;
		pa += PGSIZE;
	}

	return 0;
}

/**
 * kvmunmap - Unmap a region of memory
 * @pagetable : Base address of the target pagetable
 * @va : Virtual address
 * @size : The size of the region representing `va`
 * @do_free_pa : Whether to free the physical address
 *
 * Return: void
 */
void kvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free_pa)
{
	if (size == 0)
		return;

	if (va % PGSIZE != 0 || size % PGSIZE != 0) {
		panic("kvmunmap: va not aligned");
	}
	if (va + size < va) {
		panic("kvmunmap: address overflow");
	}

	pte_t *pte;
	uint64 a = va;
	uint64 end = a + size;
	for (; va < end; va += PGSIZE) {
		if ((pte = walk(pagetable, va, 0)) == 0) {
			continue;
			// panic("kvmunmap: walk failed");
		}
		if (!LA_PTE_IS_VALID(*pte)) {
			continue;
			// panic("kvmunmap: not mapped");
		}
		if (do_free_pa) {
			kfree((void *) KERNEL_PA2VA(LA_PTE_PA(*pte)));
		}
		*pte = 0;
	}
}

/**
 * uvmunmap - Unmap a page table
 * @pagetable : Base address of the target pagetable
 * @va : Virtual address
 * @npage : Number of pages to va
 * @do_free : Whether to free the physical memory
 *
 * Return: void
 */
void uvmunmap(pagetable_t pagetable, uint64 va, int npage, int do_free)
{
	if (npage < 0)
		panic("uvmunmap: negative page count");
	if (npage == 0)
		return;
	if (va % PGSIZE != 0)
		panic("uvmunmap: va not aligned");

	uint64 a;
	pte_t *pte;

	for (a = va; a < va + ((uint64) npage * PGSIZE); a += PGSIZE) {
		if ((pte = walk(pagetable, a, 0)) == 0) {
			continue;
		}
		if (!LA_PTE_IS_VALID(*pte)) {
			continue;
		}
		if (do_free) {
			kfree((void *) KERNEL_PA2VA(LA_PTE_PA(*pte)));
		}
		*pte = 0;
	}
}

/**
 * uvmcreate - Create a new user page table
 *
 * Context: Create a new page table and map the kernel page table to it
 *
 * Return: User page table
 */
pagetable_t uvmcreate()
{
	pagetable_t user_pagetable = (pagetable_t) kalloc();
	if (user_pagetable == 0) {
		panic("Failed to allocate memory");
	}

	return user_pagetable;
}

/**
 * uvmdealloc - Deallocate a region of memory
 * @pagetable : Base address of the target pagetable
 * @va : Virtual address must be aligned to PGSIZE
 * @size : Memory siz must be aligned to PGSIZE
 *
 * Context: This will delete an area of size `va` and free the memory.
 *
 * Return: 0 on success, -1 on error
 */
int uvmdealloc(pagetable_t pagetable, uint64 va, uint64 size)
{
	if (size == 0)
		return 0;

	uint64 old_top = va + size;
	uint64 rounded_va = PGROUNDUP(va);

	if (rounded_va < old_top) {
		uint64 rounded_old_top = PGROUNDUP(old_top);
		uint64 bytes_to_free = rounded_old_top - rounded_va;
		int npages = bytes_to_free / PGSIZE;

		uvmunmap(pagetable, rounded_va, npages, 1);
	}

	// LOG_TRACE("uvmdealloc: success");
	return 0;
}

/**
 * uvmalloc - Automatically acquire spatial data and map it
 * @pagetable : Base address of the target pagetable
 * @va : Virtual address
 * @size : Memory size
 * @perm : Permission
 *
 * Context: Will assign the size of the corresponding VA mapping,
 *
 * Return: if success, return 0, otherwise return -1
 * */
int uvmalloc(pagetable_t pagetable, uint64 va, uint64 size, uint64 perm)
{
	// LOG_TRACE("uvmalloc: va: %p, size: %d, perm: %d", (void *) va, size,
	// 	  perm);
	uint64 start = PGROUNDDOWN(va);
	uint64 end = PGROUNDUP(va + size);

	for (uint64 i = start; i < end; i += PGSIZE) {
		char *mem = kalloc();
		if (mem == 0) {
			// LOG_WARN("uvmalloc: memory allocation failed");
			uvmdealloc(pagetable, start, i - start);
			return -1;
		}

		if (mappages(pagetable, i, (uint64) KERNEL_VA2PA(mem), PGSIZE,
			     pte_from_perm(perm | PTE_USER)) < 0) {
			// LOG_WARN("uvmalloc: mappages failed");
			kfree(mem);
			uvmdealloc(pagetable, start, i - start);
			return -1;
		}
	}
	// LOG_TRACE("uvmalloc: success");
	return 0;
}

/**
 * freewalk：释放页表页，不释放页表映射的物理页。
 *
 * 该函数的参数必须是当前三级页表的根目录。根目录传入
 * freewalk_level() 时固定使用 level=2，因此不能把任意低级页表页
 * 直接作为该函数的参数。
 *
 * 返回：无。
 */
static void freewalk_level(pagetable_t pagetable, int level)
{
	for (int i = 0; i < 512; i++) {
		pte_t pte = pagetable[i];
		if (!LA_PTE_IS_VALID(pte))
			continue;

		if (level > 0) {
			/*
			 * 当前只使用 4 KiB 基本页，不使用大页，因此第 2、1 级
			 * 中的有效项必然指向下一级页表。
			 */
			uint64 child_pa = LA_PTE_PA(pte);
			freewalk_level((pagetable_t) KERNEL_PA2VA(child_pa),
				       level - 1);
			pagetable[i] = 0;
		} else {
			/*
			 * 第 0 级是叶子项。物理页由 uvmunmap() 负责释放，
			 * freewalk() 只清除页表项本身，避免重复释放物理页。
			 */
			pagetable[i] = 0;
		}
	}
	kfree((void *) pagetable);
}

void freewalk(pagetable_t pagetable)
{
	/* 当前页表固定为三级结构，入口必须是 PGDL 根目录。 */
	if (pagetable != 0)
		freewalk_level(pagetable, 2);
}

/**
 * uvmfree - Completely clear the Process page table and all the space it
 * occupies
 * @pagetable: Base address of the target pagetable
 *
 * Return: void
 */
void uvmfree(pagetable_t pagetable, struct Process *p)
{
	if (p->heap_top > 0) {
		uint64 npage = PGROUNDUP(p->heap_top) / PGSIZE;
		uvmunmap(pagetable, 0, npage, 1);
	}

	if (p->stack_top > p->stack_bottom) {
		uint64 npage =
		    PGROUNDUP(p->stack_top - p->stack_bottom) / PGSIZE;
		uvmunmap(pagetable, p->stack_bottom, npage, 1);
	}

	freewalk(pagetable);
}

/**
 * release_page_table - release the uvmcopy error new pagetable
 * */
static void release_page_table(pagetable_t pagetable, int level)
{
	for (int i = 0; i < 512; i++) {
		pte_t pte = pagetable[i];

		if (!(LA_PTE_IS_VALID(pte))) {
			continue;
		}
		if (level > 0) {
			uint64 child_pa = LA_PTE_PA(pte);
			release_page_table((pagetable_t) KERNEL_PA2VA(child_pa),
					   level - 1);
			// NOTE: not set pagetable[i] = 0
			// freeproc() will free the pagetable and set
			// pagetable[i] = 0
		} else {
			kfree((void *) KERNEL_PA2VA(LA_PTE_PA(pte)));
			pagetable[i] = 0;
		}
	}
}

/**
 * uvmcopy - Copy memory from old to new
 *
 * @old : Base address of the source pagetable
 * @new : Base address of the target pagetable
 *
 * Context: Used to copy memory from one page table to another
 *
 * Return: if success, return 0, otherwise return -1
 */
int uvmcopy(pagetable_t old, pagetable_t new)
{
	LOG_TRACE("uvmcopy: old: %p, new: %p", (void *) old, (void *) new);
	pte_t *pte2;
	pte_t *pte1;
	pte_t *pte0;
	uint64 pa;
	uint64 va;
	uint64 flags;
	char *mem;

	for (int i2 = 0; i2 < 256; i2++) {
		pte2 = &old[i2];
		if (!(LA_PTE_IS_VALID(*pte2))) {
			continue;
		}
		pagetable_t pt1 = (pagetable_t) KERNEL_PA2VA(LA_PTE_PA(*pte2));

		for (int i1 = 0; i1 < 512; i1++) {
			pte1 = &pt1[i1];
			if (!(LA_PTE_IS_VALID(*pte1))) {
				continue;
			}
			pagetable_t pt0 =
			    (pagetable_t) KERNEL_PA2VA(LA_PTE_PA(*pte1));

			for (int i0 = 0; i0 < 512; i0++) {
				pte0 = &pt0[i0];
				if (!(LA_PTE_IS_VALID(*pte0))) {
					continue;
				}

				va = ((uint64) i2 << 30 | (uint64) i1 << 21 |
				      (uint64) i0 << 12);
				pa = LA_PTE_PA(*pte0);
				pte_t original_pte = *pte0;

				if (original_pte & LA_PTE_W) {
					*pte0 = (original_pte | LA_PTE_COW) &
						~(LA_PTE_W | LA_PTE_D);
				}

				flags = loongarch_user_pte_flags(*pte0);
				sfence_vma();

				refcnt_inc(KERNEL_PA2VA(pa));

				if (mappages(new, va, pa, PGSIZE, flags) < 0) {
					*pte0 = original_pte;
					refcnt_dec(KERNEL_PA2VA(pa));
					goto err;
				}
			}
		}
	}

	sfence_vma();
	LOG_TRACE("uvmcopy: success");
	return 0;
err:
	release_page_table(new, 2);
	sfence_vma();
	LOG_TRACE("uvmcopy: failed");
	return -1;
}

/**
 * handle_anonymous_vma_fault - allocate one anonymous mmap page
 *
 * @vma : VMA covering the faulting virtual address
 * @va : Page-aligned faulting virtual address
 *
 * Context: Called from handle_vma_fault for MAP_ANONYMOUS VMAs.
 *
 * Return: 0 on success, or -1 if allocation or mapping fails
 * */
int handle_anonymous_vma_fault(struct vm_area_struct *vma, uint64 va)
{
	struct Process *proc = get_proc();
	uint64 *pa = kalloc();
	if (pa == 0) {
		return -1;
	}

	if (kvmmap(proc->pagetable, va, KERNEL_VA2PA(pa), PGSIZE,
		   vma->vm_page_prot) < 0) {
		kfree(pa);
		return -1;
	}

	sfence_vma();
	return 0;
}

/**
 * handle_file_vma_fault - fault one private read-only file-backed mmap page
 *
 * @vma : File-backed VMA covering the faulting virtual address
 * @va : Page-aligned faulting virtual address
 *
 * Context: The file offset is derived from the VMA base plus the page offset,
 * so page faults are independent of fault order. The page is cleared before
 * reading so EOF or short reads leave the remainder zero-filled.
 *
 * Return: 0 on success, or -1 if allocation, read, or mapping fails
 * */
int handle_file_vma_fault(struct vm_area_struct *vma, uint64 va)
{
	struct file *file = vma->file;
	struct Process *proc = get_proc();
	char *buf = kalloc();
	if (buf == 0) {
		return -1;
	}
	memset(buf, 0, PGSIZE);

	uint64 off = vma->file_offset + (va - vma->va_start);
	int n = vfs_read_at(file->node, off, (uint8 *) buf, PGSIZE);
	if (n < 0) {
		kfree(buf);
		return -1;
	}

	if (kvmmap(proc->pagetable, va, KERNEL_VA2PA(buf), PGSIZE,
		   vma->vm_page_prot) < 0) {
		kfree(buf);
		return -1;
	}

	sfence_vma();
	return 0;
}

/**
 * handle_vma_fault - materialize a lazy mmap page for a user fault
 *
 * @va : Faulting virtual address from the trap handler
 *
 * Context: Looks up the covering VMA, rounds the address down to a page, and
 * dispatches to the anonymous or file-backed fault path.
 *
 * Return: 0 on success, or -1 if no VMA covers the fault or mapping fails
 * */
int handle_vma_fault(uint64 va)
{
	va = PGROUNDDOWN(va);
	struct vm_area_struct *vma = find_overlapping_vma(va, PGSIZE);
	if (vma == 0) {
		LOG_WARN("handle_vma_fault: no VMA for fault va=%p",
			 (void *) va);
		return -1;
	}

	if (vma->file != 0) {
		return handle_file_vma_fault(vma, va);
	}

	return handle_anonymous_vma_fault(vma, va);
}

/**
 * copyout - Copy memory from kernel to user
 *
 * @pagetabel : Base address of the target pagetable
 * @dst : Destination address (user virtual address)
 * @src : Source address (kernel buffer)
 * @len : Number of bytes to copy, use sizeof to determine
 *
 * Context: Used to copy memory from kernel to user
 *
 * Return: if success, return 0, otherwise return -1
 */
int copyout(pagetable_t pagetable, char *dst, uint64 src, int len)
{
	if (pagetable == 0 || dst == 0 || len < 0)
		return -1;

	struct Process *current_proc = get_proc();

	uint64 user_dst = (uint64) dst;
	uint64 remaining = (uint64) len;

	if (remaining == 0)
		return 0;
	if (!loongarch_is_low_va(user_dst) || user_dst + remaining < user_dst ||
	    user_dst + remaining > LA_LOW_VA_LIMIT)
		return -1;

	while (remaining > 0) {
		uint64 va = PGROUNDDOWN(user_dst);
		pte_t *pte = walk(pagetable, va, 0);
		// NOTE: Lazily allocate the page when the PTE does not exist or
		// exists but is not yet valid (same rationale as in copyin).
		if (pte == 0 || (LA_PTE_IS_VALID(*pte)) == 0) {
			// Lazy allocation

			int is_text_data =
			    (va >= 0x10000 &&
			     va < current_proc->heap_bottom - PGSIZE);
			int is_heap = (va >= current_proc->heap_bottom &&
				       va < current_proc->heap_top);
			int is_stack = (va >= current_proc->stack_bottom &&
					va < current_proc->stack_top);

			if (is_heap || is_stack || is_text_data) {
				if (handle_page_fault(pagetable, va) < 0) {
					LOG_WARN("copyout: handle_page_fault "
						 "failed");
					return -1;
				}
			} else if (find_overlapping_vma(va, PGSIZE) != 0) {
				if (handle_vma_fault(va) < 0) {
					LOG_WARN(
					    "copyout: handle_vma_fault failed");
					return -1;
				}
			} else {
				return -1;
			}
			pte = walk(pagetable, va, 0);
		}

		if (pte != 0 && LA_PTE_IS_VALID(*pte) && (*pte & LA_PTE_COW)) {
			if (handle_cow_fault(pagetable, va) < 0)
				return -1;

			pte = walk(pagetable, va, 0);
		}

		if (pte == 0 || !LA_PTE_IS_VALID(*pte) ||
		    (*pte & (LA_PTE_W | LA_PTE_PLV3)) !=
			(LA_PTE_W | LA_PTE_PLV3)) {
			LOG_WARN("copyout: invalid or non-writable user page");
			return -1;
		}

		uint64 pa = LA_PTE_PA(*pte);
		if (pa < DRAM_BASE_LOW || pa >= PHYSTOP_LOW)
			return -1;

		uint64 offset = user_dst - va;
		uint64 size = PGSIZE - offset;
		if (size > remaining)
			size = remaining;

		memmove((void *) (KERNEL_PA2VA(pa) + offset), (void *) src,
			size);
		remaining -= size;
		user_dst += size;
		src += size;
	}

	return 0;
}

int copyin(pagetable_t pagetable, char *dst, uint64 src, int len)
{
	if (pagetable == 0 || dst == 0 || len < 0)
		return -1;

	struct Process *current_proc = get_proc();

	uint64 user_src = src;
	uint64 remaining = (uint64) len;
	if (remaining == 0)
		return 0;
	if (!loongarch_is_low_va(user_src) || user_src + remaining < user_src ||
	    user_src + remaining > LA_LOW_VA_LIMIT)
		return -1;

	while (remaining > 0) {
		uint64 va = PGROUNDDOWN(user_src);
		pte_t *pte = walk(pagetable, va, 0);

		// NOTE: Lazily allocate the page when the PTE does not exist or
		// exists but is not yet valid.  The second case occurs when
		// a prior mapping created the intermediate page-table levels
		// (e.g. for BSS) without filling the leaf PTE.
		if (pte == 0 || (LA_PTE_IS_VALID(*pte)) == 0) {
			// Lazy allocation

			int is_text_data =
			    (va >= 0x10000 &&
			     va < current_proc->heap_bottom - PGSIZE);
			int is_heap = (va >= current_proc->heap_bottom &&
				       va < current_proc->heap_top);
			int is_stack = (va >= current_proc->stack_bottom &&
					va < current_proc->stack_top);

			if (is_heap || is_stack || is_text_data) {
				if (handle_page_fault(pagetable, va) < 0) {
					LOG_WARN(
					    "copyin: handle_page_fault failed");
					return -1;
				}
			} else if (find_overlapping_vma(va, PGSIZE) != 0) {
				if (handle_vma_fault(va) < 0) {
					LOG_WARN(
					    "copyin: handle_vma_fault failed");
					return -1;
				}
			} else {
				return -1;
			}

			pte = walk(pagetable, va, 0);
		}
		if (pte == 0 || !LA_PTE_IS_VALID(*pte) ||
		    (*pte & LA_PTE_PLV3) != LA_PTE_PLV3 ||
		    (*pte & LA_PTE_NR) != 0) {
			LOG_WARN("copyin: invalid or non-readable user page");
			return -1;
		}

		uint64 pa = LA_PTE_PA(*pte);
		if (pa < DRAM_BASE_LOW || pa >= PHYSTOP_LOW)
			return -1;

		uint64 offset = user_src - va;
		uint64 size = PGSIZE - offset;
		if (size > remaining)
			size = remaining;

		memmove((void *) dst, (void *) (KERNEL_PA2VA(pa) + offset),
			size);
		remaining -= size;
		dst += size;
		user_src += size;
	}

	return 0;
}

/**
 * handle_page_fault - Handle page fault
 *
 * @pagetabel : Base address of the target pagetable
 * @va : Virtual address
 *
 * Context: Used to handle page fault
 *
 * Return: if success, return 0, otherwise return -1
 * */
int handle_page_fault(pagetable_t pagetable, uint64 va)
{
	va = PGROUNDDOWN(va);

	struct Process *current_proc = get_proc();
	if (va > current_proc->stack_bottom) {
		LOG_WARN("handle_page_fault: va out of range");
		return -1;
	}
	if (va < current_proc->heap_bottom) {
		LOG_WARN("handle_page_fault: va out of range");
		return -1;
	}

	char *mem = kalloc();
	if (mem == 0) {
		return -1;
	}
	if (mappages(pagetable, va, (uint64) KERNEL_VA2PA(mem), PGSIZE,
		     LA_PTE_P | LA_PTE_W | LA_PTE_PLV3) < 0) {
		kfree(mem);
		return -1;
	}

	sfence_vma();
	return 0;
}

int is_cow_fault(pagetable_t pagetable, uint64 va)
{
	LOG_TRACE("is_cow_fault: va: %p", (void *) va);
	va = PGROUNDDOWN(va);
	pte_t *pte = walk(pagetable, va, 0);
	if (pte == 0) {
		LOG_DEBUG("is_cow_fault: walk failed");
		return -1;
	}
	if (!(LA_PTE_IS_VALID(*pte))) {
		LOG_DEBUG("is_cow_fault: pte not valid");
		return -1;
	}
	if (*pte & LA_PTE_COW) {
		return 0;
	}
	LOG_TRACE("is_cow_fault: not a cow fault");

	return -1;
}

int handle_cow_fault(pagetable_t pagetable, uint64 va)
{
	LOG_TRACE("handle_cow_fault: va: %p", (void *) va);
	if (!loongarch_is_low_va(va))
		return -1;

	va = PGROUNDDOWN(va);
	pte_t *pte = walk(pagetable, va, 0);

	if (pte == 0 || !LA_PTE_IS_VALID(*pte) ||
	    (*pte & LA_PTE_PLV_MASK) != LA_PTE_PLV3 || !(*pte & LA_PTE_COW) ||
	    (*pte & (LA_PTE_W | LA_PTE_D)))
		return -1;

	uint64 pa = LA_PTE_PA(*pte);
	uint64 flags = loongarch_user_pte_flags(*pte);

	char *mem = kalloc();
	if (mem == 0) {
		return -1;
	}

	memmove(mem, (void *) KERNEL_PA2VA(pa), PGSIZE);
	flags &= ~LA_PTE_COW;
	flags |= LA_PTE_W | LA_PTE_D;

	kfree((void *) KERNEL_PA2VA(pa));

	*pte = LA_PA_PTE(KERNEL_VA2PA(mem)) | flags;

	sfence_vma();
	LOG_TRACE("handle_cow_fault: success");
	return 0;
}
