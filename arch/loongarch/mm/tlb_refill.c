#include "asm/boot.h"
#include "asm/loongarch.h"
#include "asm/mm.h"
#include "asm/vm.h"
#include "kernel/types.h"

/*
 * TLB 重填的 C 部分：根据硬件保存的错误地址查找三级页表，
 * 并把当前 8 KiB TLB 双页中的两个 4 KiB 页写入 TLBRELO0/1。
 *
 * TLBREHI 的低 6 位保存页大小，4 KiB 页对应 PS=12；其余部分
 * 使用双页对齐后的虚拟页号。未映射地址不能返回后重试，否则会
 * 在同一条指令上无限重复 TLB 重填，因此直接进入内核错误处理。
 */
BOOT_TEXT int boot_tlb_refill_handler(void)
{

	uint64 badva = r_tlbrbadv();
	uint64 pair_va = badva & ~((2 * PGSIZE) - 1);

	pagetable_t root;

	if (loongarch_is_high_va(badva)) {
		root = (pagetable_t) DMW0_PA2VA(r_pgdh());
	} else if (loongarch_is_low_va(badva)) {
		root = (pagetable_t) DMW0_PA2VA(r_pgdl());
	} else {
		return 0;
	}

	pte_t *pte0 = boot_walk_existing(root, pair_va);
	pte_t *pte1 = boot_walk_existing(root, pair_va + PGSIZE);

	uint64 elo0 = 0;
	uint64 elo1 = 0;

	if (pte0 != 0 && LA_PTE_IS_VALID(*pte0))
		elo0 = loongarch_pte_to_tlbelo(*pte0);

	if (pte1 != 0 && LA_PTE_IS_VALID(*pte1))
		elo1 = loongarch_pte_to_tlbelo(*pte1);

	if (elo0 == 0 && elo1 == 0) {
		boot_panic();
	}

	w_tlbrehi((pair_va & ~0x1fffULL) | LA_PAGE_SHIFT);
	w_tlbrelo0(elo0);
	w_tlbrelo1(elo1);

	return 1;
}
