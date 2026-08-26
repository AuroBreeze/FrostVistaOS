#include "asm/loongarch.h"
#include "asm/mm.h"
#include "asm/vm.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"

extern pagetable_t kernel_pgdl;

/*
 * TLB 重填的 C 部分：根据硬件保存的错误地址查找三级页表，
 * 并把当前 8 KiB TLB 双页中的两个 4 KiB 页写入 TLBRELO0/1。
 *
 * TLBREHI 的低 6 位保存页大小，4 KiB 页对应 PS=12；其余部分
 * 使用双页对齐后的虚拟页号。未映射地址不能返回后重试，否则会
 * 在同一条指令上无限重复 TLB 重填，因此直接进入内核错误处理。
 */
int tlb_refill_handler(void)
{
	uint64 badva = r_tlbrbadv();
	uint64 pair_va = badva & ~((2 * PGSIZE) - 1);
	pte_t *pte0 = walk(kernel_pgdl, pair_va, 0);
	pte_t *pte1 = walk(kernel_pgdl, pair_va + PGSIZE, 0);
	uint64 elo0 = 0;
	uint64 elo1 = 0;

	if (pte0 != 0 && (*pte0 & LA_PTE_P) != 0)
		elo0 = *pte0 & (LA_PTE_PPN_MASK | LA_PTE_P | LA_PTE_W |
				LA_PTE_PLV_MASK | LA_PTE_MAT_MASK | LA_PTE_G |
				LA_PTE_NR | LA_PTE_NX | LA_PTE_RPLV);
	if (pte1 != 0 && (*pte1 & LA_PTE_P) != 0)
		elo1 = *pte1 & (LA_PTE_PPN_MASK | LA_PTE_P | LA_PTE_W |
				LA_PTE_PLV_MASK | LA_PTE_MAT_MASK | LA_PTE_G |
				LA_PTE_NR | LA_PTE_NX | LA_PTE_RPLV);

	if (elo0 == 0 && elo1 == 0)
		panic("tlb_refill_handler: unmapped virtual address");

	/* TLBREHI.VPPN 使用双页对齐地址，PS=12 表示 4 KiB 基本页。 */
	w_tlbrehi((pair_va & ~0x1fffULL) | LA_PAGE_SHIFT);
	w_tlbrelo0(elo0);
	w_tlbrelo1(elo1);
	return 1;
}
