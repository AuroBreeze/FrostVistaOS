#ifndef __LOONGARCH_H__
#define __LOONGARCH_H__

#include "kernel/types.h"

// CRMD（0x0）：当前运行模式，包含特权级、全局中断和地址翻译模式。
static inline uint64 r_crmd()
{
	uint64 x;
	asm volatile("csrrd %0, 0x0" : "=r"(x));
	return x;
}

static inline void w_crmd(uint64 x)
{
	asm volatile("csrwr %0, 0x0" : "+r"(x));
}

// EENTRY（0xc）：普通例外和中断入口基地址。
static inline void w_eentry(uint64 x)
{
	asm volatile("csrwr %0, 0xc" : "+r"(x));
}

// ECFG（0x4）：例外入口间距和 13 路本地中断使能位。
static inline uint64 r_ecfg()
{
	uint64 x;
	asm volatile("csrrd %0, 0x4" : "=r"(x));
	return x;
}

static inline void w_ecfg(uint64 x)
{
	asm volatile("csrwr %0, 0x4" : "+r"(x));
}

// ESTAT（0x5）：例外编码、子编码和中断挂起状态。
static inline uint64 r_estat()
{
	uint64 x;
	asm volatile("csrrd %0, 0x5" : "=r"(x));
	return x;
}

// ERA（0x6）：触发例外时保存的返回地址，ERTN 从该地址恢复执行。
static inline uint64 r_era()
{
	uint64 x;
	asm volatile("csrrd %0, 0x6" : "=r"(x));
	return x;
}

static inline void w_era(uint64 era)
{
	asm volatile("csrwr %0, 0x6" : "+r"(era));
}

// BADV（0x7）：地址相关例外对应的错误虚拟地址。
static inline uint64 r_badv()
{
	uint64 x;
	asm volatile("csrrd %0, 0x7" : "=r"(x));
	return x;
}

// BADI（0x8）：触发例外的指令编码。
static inline uint64 r_badi()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8" : "=r"(x));
	return x;
}

// CPUID（0x20）：当前处理器核的逻辑编号。
static inline uint64 r_cpuid()
{
	uint64 x;
	asm volatile("csrrd %0, 0x20" : "=r"(x));
	return x;
}

// TID（0x40）：当前处理器核定时器的可编程标识符。
static inline uint64 r_tid()
{
	uint64 x;
	asm volatile("csrrd %0, 0x40" : "=r"(x));
	return x;
}

static inline void w_tid(uint64 x)
{
	asm volatile("csrwr %0, 0x40" : "+r"(x));
}

// TCFG（0x41）：定时器初值、周期模式和启用状态。
static inline uint64 r_tcfg()
{
	uint64 x;
	asm volatile("csrrd %0, 0x41" : "=r"(x));
	return x;
}

static inline void w_tcfg(uint64 x)
{
	asm volatile("csrwr %0, 0x41" : "+r"(x));
}

// TVAL（0x42）：定时器当前倒计时值，只读。
static inline uint64 r_tval()
{
	uint64 x;
	asm volatile("csrrd %0, 0x42" : "=r"(x));
	return x;
}

// CNTC（0x43）：恒定频率计数器读数的有符号补偿值。
static inline uint64 r_cntc()
{
	uint64 x;
	asm volatile("csrrd %0, 0x43" : "=r"(x));
	return x;
}

static inline void w_cntc(uint64 x)
{
	asm volatile("csrwr %0, 0x43" : "+r"(x));
}

// TICLR（0x44）：向 bit 0 写 1 清除定时器中断；读取恒为 0。
static inline uint64 r_ticlr()
{
	uint64 x;
	asm volatile("csrrd %0, 0x44" : "=r"(x));
	return x;
}

static inline void w_ticlr(uint64 x)
{
	asm volatile("csrwr %0, 0x44" : "+r"(x));
}

#endif
