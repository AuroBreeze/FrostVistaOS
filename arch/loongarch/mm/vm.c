
#include "asm/loongarch.h"
#include "asm/mm.h"
#include "asm/vm.h"
#include "kernel/defs.h"
#include "platform/uart.h"
#include "kernel/string.h"
#include "kernel/types.h"

static void loongarch_vm_selftest(pagetable_t pagetable);
static void loongarch_tlb_refill_selftest(void);
extern void tlb_entry();

/* 当前内核使用的低半地址空间页目录，供 TLB 重填入口查表。 */
pagetable_t kernel_pgdl;
static uint64 tlb_test_va = 0x00400000ULL;
static uint64 tlb_test_pa;

/*
 * 查找低半地址空间中虚拟地址对应的最终页表项。
 *
 * level=2：PGDL 下的 Dir2
 * level=1：Dir2 下的 Dir1
 * level=0：Dir1 下的最终页表 PT
 *
 * 中间目录项不存在时，alloc 非零表示分配并清零新的页表页。
 * 目录项中的页表地址必须写入物理地址，访问页表内容时则使用 DMW0
 * 高地址别名。
 */
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc)
{
	if (pagetable == 0 || va >= LA_LOW_VA_LIMIT) {
		return 0;
	}

	for (int level = 2; level > 0; level--) {
		pte_t *pte = &pagetable[loongarch_vpn(va, level)];
		if (*pte & LA_PTE_P) {
			uint64 child_pa = LA_PTE_PA(*pte);
			pagetable = (pagetable_t) PA2VA(child_pa);
			continue;
		}

		if (!alloc) {
			return 0;
		}

		pagetable_t child = (pagetable_t) kalloc();
		if (child == 0) {
			return 0;
		}

		/* kalloc() 返回 DMW0 高地址，并已清零整页。 */
		*pte = LA_PA_PTE(VA2PA((uint64) child)) | LA_PTE_P | LA_PTE_W;
		pagetable = child;
	}

	return &pagetable[loongarch_vpn(va, 0)];
}

static uint64 loongarch_pwcl_3level(void)
{
	return LA_PWCL_FIELD(LA_PAGE_SHIFT, 0) | LA_PWCL_FIELD(LA_PT_WIDTH, 5) |
	       LA_PWCL_FIELD(LA_DIR1_BASE, 10) |
	       LA_PWCL_FIELD(LA_DIR1_WIDTH, 15) |
	       LA_PWCL_FIELD(LA_DIR2_BASE, 20) |
	       LA_PWCL_FIELD(LA_DIR2_WIDTH, 25);
}

/* 保留原有的架构页表配置入口 */
uint64 setup_paging()
{
	return loongarch_pwcl_3level();
}

void paging_init()
{
	/* 页面大小为 4096 字节，即 2^12 */
	w_stlbps(LA_PAGE_SHIFT);
	/* TLB 例外入口配置 */
	/* TLBRENTRY 要求填写物理地址；入口符号链接在 DMW0 高地址。 */
	w_tlbrentry(VA2PA((uint64) tlb_entry));
	/* 低半地址空间三级页表配置 */
	w_pwcl(setup_paging());
	/* 清除启动前可能残留的 TLB 项，确保后续测试确实触发重填。 */
	invtlb_all();
}

void loongarch_vm_init()
{
	pagetable_t pgdl = (pagetable_t) kalloc();
	if (pgdl == 0) {
		panic("loongarch_vm_init: cannot allocate PGDL");
	}

	/* 页表内存通过 DMW0 高地址别名访问 */
	memset(pgdl, 0, PGSIZE);
	kernel_pgdl = pgdl;

	/* 配置 4 KiB 页面以及 PT + Dir1 + Dir2 三级索引 */
	paging_init();

	/* 页表遍历期间，PGDL 和各级目录指针使用物理地址 */
	w_pgdl(VA2PA((uint64) pgdl));
	loongarch_vm_selftest(pgdl);

	/* 启用页表翻译时继续保留 DMW0/DMW1 */
	uint64 crmd = r_crmd();
	crmd &= ~CRMD_DA;
	crmd |= CRMD_PG;
	w_crmd(crmd);

	asm volatile("ibar 0" ::: "memory");
	loongarch_tlb_refill_selftest();
	kprintf("LoongArch: enabled 3-level lower-half page table\n");
}

int mappages(pagetable_t pagetable, uint64 va, uint64 pa, uint64 size,
	     uint64 perm)
{
	if (va % PGSIZE != 0 || pa % PGSIZE != 0 || size % PGSIZE != 0)
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

		if (*pte & LA_PTE_P) {
			panic("mappages: remap");
		}

		*pte = LA_PA_PTE(pa) | perm | LA_PTE_P | LA_PTE_MAT_CC;
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
	if ((*pte & LA_PTE_P) == 0) {
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
int kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, int size, int perm)
{
	return mappages(pagetable, va, pa, size, perm);
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
	if (va % PGSIZE != 0 || size % PGSIZE != 0) {
		panic("kvmunmap: va not aligned");
	}

	pte_t *pte;
	uint64 a = va;
	for (; va < a + size; va += PGSIZE) {
		if ((pte = walk(pagetable, va, 0)) == 0) {
			continue;
			// panic("kvmunmap: walk failed");
		}
		if ((*pte & LA_PTE_P) == 0) {
			continue;
			// panic("kvmunmap: not mapped");
		}
		if (do_free_pa) {
			kfree((void *) PA2VA(LA_PTE_PA(*pte)));
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
	uint64 a;
	pte_t *pte;

	for (a = va; a < va + ((uint64) npage * PGSIZE); a += PGSIZE) {
		if ((pte = walk(pagetable, a, 0)) == 0) {
			continue;
		}
		if ((*pte & LA_PTE_P) == 0) {
			continue;
		}
		if (do_free) {
			kfree((void *) PA2VA(LA_PTE_PA(*pte)));
		}
		*pte = 0;
	}
}

/*
 * 第三步页表软件自检：建立一个 4 KiB 低地址映射，并验证反向查找。
 * 这里只验证页表结构，不访问低地址，因此不依赖 TLB 重填处理器。
 */
static void loongarch_vm_selftest(pagetable_t pagetable)
{
	char *test_page = (char *) kalloc();
	if (test_page == 0) {
		panic("loongarch_vm_selftest: cannot allocate test page");
	}

	tlb_test_pa = VA2PA((uint64) test_page);
	if (mappages(pagetable, tlb_test_va, tlb_test_pa, PGSIZE,
		     LA_PTE_W | LA_PTE_PLV0) < 0) {
		panic("loongarch_vm_selftest: mappages failed");
	}

	uint64 resolved_pa = walk_addr(pagetable, tlb_test_va);
	if (resolved_pa != tlb_test_pa) {
		panic("loongarch_vm_selftest: translation mismatch");
	}

	kprintf("LoongArch: 3-level page-table self-test passed\n");
}

/*
 * 打开分页后访问低半地址，验证硬件 TLB 重填入口和 TLBFILL 路径。
 * 该地址不能使用 DMW 高地址，否则会绕过页表和 TLB，测试没有意义。
 */
static void loongarch_tlb_refill_selftest(void)
{
	volatile uint64 *test_va = (volatile uint64 *) tlb_test_va;
	const uint64 pattern = 0x1122334455667788ULL;

	/* 第一次访问应触发 TLB 重填，返回后重新执行这条存储指令。 */
	*test_va = pattern;

	/* 读取应命中刚刚填入的 TLB 双页项。 */
	if (*test_va != pattern)
		panic("loongarch_tlb_refill_selftest: data mismatch");

	kprintf("LoongArch: TLB refill self-test passed\n");
}
