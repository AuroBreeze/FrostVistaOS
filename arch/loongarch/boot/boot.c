#define LOG_MODULE "BOOT"

#include "asm/vm.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"
#include "asm/loongarch.h"
#include "platform/timer.h"
#include "platform/uart.h"
#include "asm/trap.h"
#include "asm/mm.h"
#include "asm/boot.h"

#define BOOT_PT_ENTRIES (PGSIZE / sizeof(pte_t))

/*
 * 页表初始化发生在 kalloc_init() 之前，因此不能从普通分配器取页。
 * 这块内存被放进 .bss.boot，启动阶段通过 DMW0 直接访问；前两个页
 * 分别作为 PGDL 和 PGDH，其余页供 boot_map_kernel() 建立下级目录。
 *
 * 96 页足以覆盖当前 126 MiB 内存模型的三级高半区页表，并为内核
 * 镜像和早期扩展留出余量。后续切换到正式内存管理器后，这个池仍然
 * 属于启动保留区，不会被 kalloc_init() 回收。
 */
#define BOOT_PT_PAGES 96

static pte_t boot_page_table_pool[BOOT_PT_PAGES * BOOT_PT_ENTRIES] BOOT_BSS
    __attribute__((aligned(PGSIZE)));
static uint64 boot_page_table_next BOOT_BSS;

static const char boot_banner[] BOOT_RODATA =
    "\r\n[FrostVista] LoongArch boot\r\n";

BOOT_TEXT void boot_panic(void)
{
	boot_uart_puts("boot failed\r\n");

	for (;;) {
	}
}

static BOOT_TEXT void boot_zero_page(pte_t *page)
{
	for (uint64 i = 0; i < BOOT_PT_ENTRIES; i++)
		page[i] = 0;
}

static BOOT_TEXT pagetable_t boot_alloc_page(void)
{
	if (boot_page_table_next >= BOOT_PT_PAGES)
		return 0;

	pagetable_t page =
	    &boot_page_table_pool[boot_page_table_next * BOOT_PT_ENTRIES];
	boot_page_table_next++;
	boot_zero_page(page);
	return page;
}

static BOOT_TEXT pagetable_t boot_pgdl(void)
{
	return &boot_page_table_pool[0 * BOOT_PT_ENTRIES];
}

static BOOT_TEXT pagetable_t boot_pgdh(void)
{
	return &boot_page_table_pool[1 * BOOT_PT_ENTRIES];
}

static BOOT_TEXT uint64 boot_pwcl(void)
{
	/* PT + Dir1 + Dir2，分别占用 4 KiB、2 MiB、1 GiB 的索引层。 */
	return LA_PWCL_FIELD(LA_PAGE_SHIFT, 0) | LA_PWCL_FIELD(LA_PT_WIDTH, 5) |
	       LA_PWCL_FIELD(LA_DIR1_BASE, 10) |
	       LA_PWCL_FIELD(LA_DIR1_WIDTH, 15) |
	       LA_PWCL_FIELD(LA_DIR2_BASE, 20) |
	       LA_PWCL_FIELD(LA_DIR2_WIDTH, 25);
}

static BOOT_TEXT pte_t *boot_walk(pagetable_t root, uint64 va)
{
	pagetable_t pagetable = root;

	if (pagetable == 0)
		return 0;

	if (!loongarch_is_high_va(va))
		return 0;

	for (int level = 2; level > 0; level--) {
		pte_t *pte = &pagetable[loongarch_vpn(va, level)];

		if (LA_PTE_IS_VALID(*pte)) {
			uint64 child_pa = LA_PTE_PA(*pte);

			/*
			 * 页表项保存物理地址，但是访问下一级页表时要使用
			 * DMW0 下的虚拟地址。
			 */
			pagetable = (pagetable_t) DMW0_PA2VA(child_pa);
			continue;
		}

		pagetable_t child = boot_alloc_page();
		if (child == 0)
			return 0;

		uint64 child_pa = DMW0_VA2PA((uint64) child);

		/*
		 * 目录项中写入下一级页表的物理地址。
		 */
		*pte = LA_PA_PTE(child_pa) | LA_PTE_V | LA_PTE_P;

		pagetable = child;
	}

	return &pagetable[loongarch_vpn(va, 0)];
}

/*
 * 查找已经建立的高半区页表项。
 *
 * 与 boot_walk() 不同，这个版本绝不会分配新的页表页，适合在 TLB
 * 重填异常中使用。异常路径不能依赖普通分配器，也不应该在处理一个
 * 缺页时递归建立页表。
 */
BOOT_TEXT pte_t *boot_walk_existing(pagetable_t root, uint64 va)
{
	pagetable_t pagetable = root;

	if (pagetable == 0 ||
	    (!loongarch_is_high_va(va) && !loongarch_is_low_va(va))) {
		return 0;
	}
	for (int level = 2; level > 0; level--) {
		pte_t *pte = &pagetable[loongarch_vpn(va, level)];

		if (!LA_PTE_IS_VALID(*pte))
			return 0;

		/* 页表项保存物理地址，访问下一级页表时使用 DMW0 别名。 */
		pagetable = (pagetable_t) DMW0_PA2VA(LA_PTE_PA(*pte));
	}

	return &pagetable[loongarch_vpn(va, 0)];
}

static BOOT_TEXT int boot_mappages(pagetable_t pagetable, uint64 va, uint64 pa,
				   uint64 size, uint64 perm)
{
	if (size == 0) {
		boot_uart_puts("size == 0");
		return -1;
	}

	if ((va & (PGSIZE - 1)) != 0 || (pa & (PGSIZE - 1)) != 0 ||
	    (size & (PGSIZE - 1)) != 0) {
		boot_uart_puts("va pa size mode PGSIZE error");
		return -1; /* 防止计算映射末尾地址时发生无符号整数溢出。 */
	}
	if (va + size < va || pa + size < pa) {
		boot_uart_puts("va + size < va || pa + size < pa");
		return -1;
	}

	uint64 a;
	uint64 last;
	pte_t *pte;

	a = va;
	last = va + size - PGSIZE;

	for (;;) {
		if ((pte = boot_walk(pagetable, a)) == 0) {
			boot_uart_puts("boot_walk error");
			return -1;
		}

		if (LA_PTE_IS_VALID(*pte)) {
			boot_panic();
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

BOOT_TEXT void loongarch_bootstrap(void)
{
	/*
	 * Keep the pre-paging call graph explicit.  These are placeholders for
	 * now; each implementation must remain callable through DMW0 until the
	 * final high-half mapping is active.
	 */
	boot_uart_init();
	boot_uart_puts(boot_banner);

	boot_setup_page_tables();
	boot_uart_puts("[BOOT] page tables: ready\r\n");

	boot_map_kernel();
	boot_uart_puts("[BOOT] kernel mapping: ready\r\n");

	boot_setup_tlb_refill();
	boot_uart_puts("[BOOT] TLB refill: ready\r\n");

	boot_enable_paging();
	// boot_uart_puts("boot_enable_paging end\r\n");

	boot_jump_to_high();

	/* The jump stage is a placeholder and therefore returns for now. */
	for (;;) {
	}
}

BOOT_TEXT void boot_setup_page_tables(void)
{
	/* 根表固定占用 boot 页表池的前两个页。 */
	boot_page_table_next = 2;
	boot_zero_page(boot_pgdl());
	boot_zero_page(boot_pgdh());

	/*
	 * 高半区仍然使用同一个三级布局；PGDH 只负责选择另一棵根表。
	 * PWCH 为 0 表示不额外增加 Dir3/Dir4 层。
	 */
	w_stlbps(LA_PAGE_SHIFT);
	w_pwcl(boot_pwcl());
	w_pwch(0);

	/* CSR 保存物理根地址，不是 DMW0 下的虚拟地址。 */
	w_pgdl(DMW0_VA2PA((uint64) boot_pgdl()));
	w_pgdh(DMW0_VA2PA((uint64) boot_pgdh()));
}

BOOT_TEXT void boot_map_kernel(void)
{
	extern char _text_start[];
	extern char _text_start_pa[];
	extern char _text_end[];

	extern char _data_start[];
	extern char _data_start_pa[];
	extern char _data_end[];

	extern char _bss_start[];
	extern char _bss_start_pa[];
	extern char _bss_end_pa[];

	extern char _kernel_end[];
	extern char _kernel_end_pa[];

	pagetable_t pagetable = boot_pgdh();
	uint64 low_ram_pa = DRAM_BASE_LOW;
	uint64 low_ram_va = KERNEL_PA2VA(low_ram_pa);

	uint64 text_va = (uint64) _text_start;
	uint64 text_pa = (uint64) _text_start_pa;
	uint64 text_size = (uint64) _text_end - (uint64) _text_start;
	uint64 low_ram_size = text_pa - low_ram_pa;

	uint64 data_va = (uint64) _data_start;
	uint64 data_pa = (uint64) _data_start_pa;
	uint64 data_size = (uint64) _data_end - (uint64) _data_start;

	uint64 bss_va = (uint64) _bss_start;
	uint64 bss_pa = (uint64) _bss_start_pa;
	uint64 bss_size = (uint64) _bss_end_pa - (uint64) _bss_start_pa;

	uint64 free_va = (uint64) _kernel_end;
	uint64 free_pa = (uint64) _kernel_end_pa;
	uint64 free_size = PHYSTOP_LOW - free_pa;

	boot_uart_puts("[BOOT] kernel mapping: begin\r\n");
	/*
	 * 链接脚本已经保证这些区域按页对齐。
	 */
	if ((text_va & (PGSIZE - 1)) != 0 || (text_pa & (PGSIZE - 1)) != 0 ||
	    (text_size & (PGSIZE - 1)) != 0) {
		boot_uart_puts("1\r\n");
		boot_panic();
	}
	if (text_pa < low_ram_pa || (low_ram_va & (PGSIZE - 1)) != 0 ||
	    (low_ram_size & (PGSIZE - 1)) != 0) {
		boot_uart_puts("0\\r\\n");
		boot_panic();
	}

	if ((data_va & (PGSIZE - 1)) != 0 || (data_pa & (PGSIZE - 1)) != 0 ||
	    (data_size & (PGSIZE - 1)) != 0) {
		boot_uart_puts("2\r\n");
		boot_panic();
	}

	if ((bss_va & (PGSIZE - 1)) != 0 || (bss_pa & (PGSIZE - 1)) != 0 ||
	    (bss_size & (PGSIZE - 1)) != 0) {
		boot_uart_puts("3\r\n");
		boot_panic();
	}

	/*
	 * 建立完整的高半区 RAM 直接映射。内核镜像前的启动区不在正式
	 * 调用路径中，因此使用 RW + NX；随后分段覆盖内核镜像权限。
	 */
	if (low_ram_size != 0 &&
	    boot_mappages(pagetable, low_ram_va, low_ram_pa, low_ram_size,
			  LA_PTE_PLV0 | LA_PTE_W | LA_PTE_NX) < 0) {
		boot_uart_puts("8\\r\\n");
		boot_panic();
	}

	if (boot_mappages(pagetable, text_va, text_pa, text_size, LA_PTE_PLV0) <
	    0) {
		boot_uart_puts("4\r\n");
		boot_panic();
	}

	/*
	 * .data 可读写、不可执行。
	 */
	if (boot_mappages(pagetable, data_va, data_pa, data_size,
			  LA_PTE_PLV0 | LA_PTE_W | LA_PTE_NX) < 0) {
		boot_uart_puts("5\r\n");
		boot_panic();
	}

	/*
	 * .bss 范围包含正式内核栈。
	 */
	if (boot_mappages(pagetable, bss_va, bss_pa, bss_size,
			  LA_PTE_PLV0 | LA_PTE_W | LA_PTE_NX) < 0) {
		boot_uart_puts("6\r\n");
		boot_panic();
	}

	if (free_pa < PHYSTOP_LOW) {
		if (boot_mappages(pagetable, free_va, free_pa, free_size,
				  LA_PTE_PLV0 | LA_PTE_W | LA_PTE_NX) < 0) {
			boot_uart_puts("7\r\n");
			boot_panic();
		}
	}
}

BOOT_TEXT void boot_setup_tlb_refill(void)
{
	extern void tlb_entry();
	uint64 entry_pa = DMW0_VA2PA((uint64) tlb_entry);

	w_tlbrentry(entry_pa);
	invtlb_all();

	asm volatile("dbar 0\n\tibar 0" ::: "memory");
}

BOOT_TEXT void boot_enable_paging(void)
{
	boot_uart_puts("[BOOT] paging: enabling\r\n");

	uint64 crmd = r_crmd();
	crmd &= ~CRMD_DA;
	crmd |= CRMD_PG;
	w_crmd(crmd);

	asm volatile("dbar 0\n\tibar 0" ::: "memory");

	boot_uart_puts("[BOOT] paging: active\r\n");
}

extern void kernelvec(void);
void trap_init(void)
{
	// EENTRY 保存普通例外和中断的入口基地址。
	w_eentry((uint64) kernelvec);

	// VS=0：所有普通例外和中断共用 EENTRY，由软件读取 ESTAT 分发。
	w_ecfg(ECFG_VS(0));
}

static void display_banner(void)
{
	LOG_SEP();
	LOG_BANNER("    ______                __ _    ___      __       ");
	LOG_BANNER("   / ____/________  _____/ /| |  / (_)____/ /_____ _");
	LOG_BANNER("  / /_  / ___/ __ \\/ ___/ __/ | / / / ___/ __/ __ `/");
	LOG_BANNER(" / __/ / /  / /_/ (__  ) /_ | |/ / (__  ) /_/ /_/ / ");
	LOG_BANNER("/_/   /_/   \\____/____/\\__/ |___/_/____/\\__/\\__,_/");
	LOG_BANNER("");
	LOG_BANNER("LoongArch 64  |  LA64  |  39-bit VA  |  v1.0");
	LOG_SEP();
}

void loong_early_boot(void)
{
	trap_init();
	uart_init();

	display_banner();
	LOG_TRACE("Successfully entered the high-half kernel");
	LOG_TRACE("Current CPUID: %d", cpuid());
	{
		uint64 current_sp;
		asm volatile("move %0, $sp" : "=r"(current_sp));
		LOG_TRACE("Current SP: %p", current_sp);
	}

	LOG_PHASE("Platform Init");
	LOG_INFO("Exception vector initialized");

	timer_init();
	LOG_INFO("Timer initialized");

	LOG_PHASE("Memory and Process Subsystem");
	procinit();
	kalloc_init();

	extern void slab_init(void);
	slab_init();
	kmalloc_cache_init();

	LOG_INFO("CPU-local process state initialized");

	LOG_PHASE("Device Subsystem");
	device_mapping();

	LOG_PHASE("Kernel Ready");
	LOG_PHASE("Hello FrostVista OS!");

	for (;;)
		;
}
