#define LOG_MODULE "MM"

#include "asm/mm.h"
#include "asm/loongarch.h"
#include "asm/vm.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "platform/uart.h"
#include "kernel/string.h"
#include "kernel/types.h"
#include "kernel/proc.h"

void device_mapping()
{
	if (kvmmap_mmio_current(UART_PAGE_VA, UART_PAGE_PA, PGSIZE,
				LA_PTE_PLV0 | LA_PTE_W | LA_PTE_NX |
				    LA_PTE_MAT_SUC) < 0) {
		panic("device_mapping: map UART failed");
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
 * uvmfree - Completely clear the page table and all the space it occupies
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

int uvmcopy(pagetable_t old, pagetable_t new)
{
	for (uint64 i2 = 0; i2 < 256; i2++) {
		pte_t *old_pte2 = &old[i2];

		if (!LA_PTE_IS_VALID(*old_pte2))
			continue;

		pagetable_t old_pt1 =
		    (pagetable_t) KERNEL_PA2VA(LA_PTE_PA(*old_pte2));

		for (uint64 i1 = 0; i1 < 512; i1++) {
			pte_t *old_pte1 = &old_pt1[i1];

			if (!LA_PTE_IS_VALID(*old_pte1))
				continue;

			pagetable_t old_pt0 =
			    (pagetable_t) KERNEL_PA2VA(LA_PTE_PA(*old_pte1));

			for (uint64 i0 = 0; i0 < 512; i0++) {
				pte_t *old_pte0 = &old_pt0[i0];

				if (!LA_PTE_IS_VALID(*old_pte0))
					continue;

				uint64 va =
				    (i2 << 30) | (i1 << 21) | (i0 << 12);

				uint64 old_pa = LA_PTE_PA(*old_pte0);
				uint64 flags =
				    loongarch_user_pte_flags(*old_pte0);

				char *mem = kalloc();
				if (mem == 0)
					goto err;

				memcpy(mem, (void *) KERNEL_PA2VA(old_pa),
				       PGSIZE);

				pte_t *new_pte = walk(new, va, 1);
				if (new_pte == 0 || LA_PTE_IS_VALID(*new_pte)) {
					kfree(mem);
					goto err;
				}

				uint64 new_pa = KERNEL_VA2PA((uint64) mem);

				if (flags & LA_PTE_W)
					flags |= LA_PTE_D;

				*new_pte = LA_PA_PTE(new_pa) | flags |
					   LA_PTE_V | LA_PTE_P;
			}
		}
	}

	return 0;

err:
	/*
	 * 这里要释放已经复制到 new 中的用户页，
	 * 然后释放 new 的页表页。
	 */
	return -1;
}
