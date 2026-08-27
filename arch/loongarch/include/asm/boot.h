#ifndef __LOONGARCH_BOOT_H
#define __LOONGARCH_BOOT_H

#include "kernel/types.h"

/*
 * Functions and data used before the kernel switches to its final address
 * space.  The linker places these sections in the DMW0-accessible .boot
 * output section.
 */
#define BOOT_TEXT __attribute__((section(".text.boot"), noinline, used))

#define BOOT_DATA __attribute__((section(".data.boot")))

#define BOOT_BSS __attribute__((section(".bss.boot")))

#define BOOT_RODATA __attribute__((section(".rodata.boot")))

/* Early boot stages.  The implementations are intentionally small so that
 * each stage can later be filled in without pulling normal kernel code into
 * the pre-paging call graph. */
BOOT_TEXT void boot_setup_page_tables(void);
BOOT_TEXT void boot_map_kernel(void);
BOOT_TEXT void boot_setup_tlb_refill(void);
BOOT_TEXT void boot_enable_paging(void);
BOOT_TEXT void boot_jump_to_high(void);
BOOT_TEXT void boot_panic(void);
BOOT_TEXT pte_t *boot_walk_existing(pagetable_t root, uint64 va);
BOOT_TEXT int boot_tlb_refill_handler(void);

BOOT_TEXT void boot_uart_init(void);
BOOT_TEXT void boot_uart_putc(char c);
BOOT_TEXT void boot_uart_puts(const char *s);

BOOT_TEXT void loongarch_bootstrap(void);

#endif
