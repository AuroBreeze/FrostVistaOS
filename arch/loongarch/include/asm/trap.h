#ifndef __LOONGARCH_TRAP_H
#define __LOONGARCH_TRAP_H

#include "kernel/types.h"

// ECFG.VS 位于 [18:16]，控制向量入口间距为 2^VS 条指令。
// 当前设置 VS=0，使所有普通例外和中断进入同一个 kernelvec。
#define ECFG_VS_SHIFT 16
#define ECFG_VS(x) ((x) << ECFG_VS_SHIFT)

// ESTAT 中 Ecode[21:16] 保存一级例外编码，EsubCode[30:22]
// 保存二级例外编码，IS[12:0] 保存各路中断的挂起状态。
#define ESTAT_ECODE_SHIFT 16
#define ESTAT_ESUBCODE_SHIFT 22
#define ESTAT_IS_MASK 0x1fff	  // 12:0
#define ESTAT_ECODE_MASK 0x3f	  // 21:16
#define ESTAT_ESUBCODE_MASK 0x1ff // 30:22

/* LoongArch 一级异常编码（ESTAT.Ecode）。 */
#define LA_ECODE_PIL 0x1  /* load page invalid，加载页无效 */
#define LA_ECODE_PIS 0x2  /* store page invalid，存储页无效 */
#define LA_ECODE_PIF 0x3  /* fetch page invalid，取指页无效 */
#define LA_ECODE_PME 0x4  /* page modification，页修改异常 */
#define LA_ECODE_PNR 0x5  /* page non-readable，页不可读 */
#define LA_ECODE_PNX 0x6  /* page non-executable，页不可执行 */
#define LA_ECODE_PPI 0x7  /* page privilege illegal，页权限非法 */
#define LA_ECODE_ADEF 0x8 /* address error on fetch，取指地址错误 */
#define LA_ECODE_ALE 0x9  /* address alignment error，地址未对齐 */
#define LA_ECODE_BCE 0xa  /* bound check error，边界检查异常 */
#define LA_ECODE_SYS 0xb  /* system call，系统调用 */
#define LA_ECODE_BRK 0xc  /* breakpoint，断点异常 */
#define LA_ECODE_INE 0xd  /* instruction undefined，指令未定义 */
#define LA_ECODE_IPE 0xe  /* instruction privilege error，指令权限错误 */
#define LA_ECODE_FPD 0xf  /* floating-point disabled，浮点指令禁用 */

static inline int is_interrupt(uint64 estat)
{
	// 在当前 VS=0 的统一入口模式下，Ecode=0 表示中断。
	return ((estat >> ESTAT_ECODE_SHIFT) & ESTAT_ECODE_MASK) == 0;
}

static inline uint64 estat_ecode(uint64 estat)
{
	return (estat >> ESTAT_ECODE_SHIFT) & ESTAT_ECODE_MASK;
}

static inline uint64 estat_esubcode(uint64 estat)
{
	return (estat >> ESTAT_ESUBCODE_SHIFT) & ESTAT_ESUBCODE_MASK;
}

static inline uint64 estat_is(uint64 estat)
{
	return estat & ESTAT_IS_MASK;
}

static inline int estat_is_page_fault(uint64 estat)
{
	uint64 ecode = estat_ecode(estat);
	return ecode == LA_ECODE_PIL || ecode == LA_ECODE_PIS ||
	       ecode == LA_ECODE_PIF;
}

#endif
