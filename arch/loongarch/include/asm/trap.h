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

struct arch_trapframe {
	// $zero 恒为 0，无需保存到异常现场。
	// uint64 zero;
	uint64 ra;
	uint64 tp;
	uint64 sp;

	uint64 a0;
	uint64 a1;
	uint64 a2;
	uint64 a3;
	uint64 a4;
	uint64 a5;
	uint64 a6;
	uint64 a7;

	uint64 t0;
	uint64 t1;
	uint64 t2;
	uint64 t3;
	uint64 t4;
	uint64 t5;
	uint64 t6;
	uint64 t7;
	uint64 t8;

	uint64 u0;

	uint64 fp;

	uint64 s0;
	uint64 s1;
	uint64 s2;
	uint64 s3;
	uint64 s4;
	uint64 s5;
	uint64 s6;
	uint64 s7;
	uint64 s8;
};
#endif
