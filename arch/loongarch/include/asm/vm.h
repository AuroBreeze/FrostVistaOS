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
 * 页表根目录写入 PGDL。当前 LoongArch 仍处于 bring-up 阶段，内核和
 * MMIO 继续使用 DMW0/DMW1，因此这里暂不创建用户地址映射
 */
#include "kernel/types.h"
#define LA_PAGE_SHIFT 12 // 4KB页面
#define LA_PT_WIDTH 9
#define LA_DIR1_BASE 21
#define LA_DIR1_WIDTH 9
#define LA_DIR2_BASE 30
#define LA_DIR2_WIDTH 9
#define LA_PT_ENTRIES (1 << LA_PT_WIDTH)
#define LA_LOW_VA_BITS                                                         \
	(LA_PAGE_SHIFT + LA_PT_WIDTH + LA_DIR1_WIDTH + LA_DIR2_WIDTH)
#define LA_LOW_VA_LIMIT (1ULL << LA_LOW_VA_BITS)

#define LA_PWCL_FIELD(value, shift) ((uint64) (value) << (shift))

/* CRMD 地址翻译模式相关位 */
#define CRMD_DA (1ULL << 3)
#define CRMD_PG (1ULL << 4)

static inline uint64 loongarch_vpn(uint64 va, int level)
{
	return (va >> (LA_PAGE_SHIFT + (level * LA_PT_WIDTH))) &
	       (LA_PT_ENTRIES - 1);
}

#endif
