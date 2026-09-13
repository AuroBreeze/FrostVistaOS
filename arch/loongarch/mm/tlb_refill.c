#include "asm/boot.h"
#include "asm/loongarch.h"
#include "asm/mm.h"
#include "asm/vm.h"
#include "kernel/types.h"

/*
 * C portion of the TLB refill path: walk the three-level page table using the
 * fault address saved by hardware, then write both 4 KiB pages in the current
 * 8 KiB TLB pair to TLBRELO0 and TLBRELO1.
 *
 * The low six bits of TLBREHI hold the page size, with PS=12 representing a
 * 4 KiB page; the remaining bits contain the virtual page number aligned to a
 * page pair. An unmapped address cannot return for a retry because that would
 * repeatedly refill the TLB for the same instruction, so it enters the kernel
 * error path instead.
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

	uint64 previous_plv = r_tlbrprmd() & PRMD_PPLV_MASK;

	if (elo0 == 0 && elo1 == 0 && previous_plv != PRMD_PPLV_PLV3) {
		boot_panic();
	}

	w_tlbrehi((pair_va & ~0x1fffULL) | LA_PAGE_SHIFT);
	w_tlbrelo0(elo0);
	w_tlbrelo1(elo1);

	return 1;
}
