#ifndef __LOONGARCH_VM_H
#define __LOONGARCH_VM_H

/*
 * 为低半地址空间配置三级页表：
 *
 *   VA[38:30] -> Dir2
 *   VA[29:21] -> Dir1
 *   VA[20:12] -> PT
 *   VA[11:0]  -> 页内偏移
 *
 * 当前活动地址空间使用 PGDL/PGDH 分别管理低半区和高半区地址。
 */
#include "kernel/types.h"

struct Process;

#define LA_PAGE_SHIFT 12 // 4KB页面
#define LA_PT_WIDTH 9
#define LA_DIR1_BASE 21
#define LA_DIR1_WIDTH 9
#define LA_DIR2_BASE 30
#define LA_DIR2_WIDTH 9
#define LA_PT_ENTRIES (1 << LA_PT_WIDTH)
/*
 * 三级 9-bit 页表加 4 KiB 页使用 VA[38:0]，故 VALEN 为 39。
 * VA[38] 是符号位：低半区是 [0, 2^38)，高半区从
 * 0xffffffc000000000 开始；其余地址均为非规范地址。
 */
#define LA_VALEN (LA_PAGE_SHIFT + LA_PT_WIDTH + LA_DIR1_WIDTH + LA_DIR2_WIDTH)
#define LA_VA_SIGN_BIT (LA_VALEN - 1)
#define LA_LOW_VA_LIMIT (1ULL << LA_VA_SIGN_BIT)
#define LA_HIGH_VA_BASE (~(LA_LOW_VA_LIMIT - 1ULL))

static inline int loongarch_is_low_va(uint64 va)
{
	return va < LA_LOW_VA_LIMIT;
}

static inline int loongarch_is_high_va(uint64 va)
{
	return va >= LA_HIGH_VA_BASE;
}

#define LA_PWCL_FIELD(value, shift) ((uint64) (value) << (shift))

/* CRMD 地址翻译模式相关位 */
#define CRMD_DA (1ULL << 3)
#define CRMD_PG (1ULL << 4)

static inline uint64 loongarch_vpn(uint64 va, int level)
{
	return (va >> (LA_PAGE_SHIFT + (level * LA_PT_WIDTH))) &
	       (LA_PT_ENTRIES - 1);
}

int kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 size,
	   uint64 perm);
int kvmmap_mmio_current(uint64 va, uint64 pa, uint64 size, uint64 perm);

pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
pte_t *walk_current(uint64 va, int alloc);
int mappages(pagetable_t pagetable, uint64 va, uint64 pa, uint64 size,
	     uint64 perm);
uint64 walk_addr(pagetable_t pagetable, uint64 va);
void kvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free_pa);
void uvmunmap(pagetable_t pagetable, uint64 va, int npage, int do_free);
pagetable_t uvmcreate(void);
int uvmdealloc(pagetable_t pagetable, uint64 va, uint64 size);
int uvmalloc(pagetable_t pagetable, uint64 va, uint64 size, uint64 perm);
void freewalk(pagetable_t pagetable);
void uvmfree(pagetable_t pagetable, struct Process *p);
int uvmcopy(pagetable_t old, pagetable_t new);
int copyout(pagetable_t pagetable, char *dst, uint64 src, int len);
int copyin(pagetable_t pagetable, char *dst, uint64 src, int len);

void device_mapping();
#endif
