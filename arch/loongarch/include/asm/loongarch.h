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

// DMW1（0x181）：启动期非缓存 MMIO 直接映射窗口。
static inline uint64 r_dmw1()
{
	uint64 x;
	asm volatile("csrrd %0, 0x181" : "=r"(x));
	return x;
}

static inline void w_dmw1(uint64 x)
{
	asm volatile("csrwr %0, 0x181" : "+r"(x));
}

// TLBIDX（0x10）：TLB 索引、页大小和条目有效状态等信息。
static inline uint64 r_tlbidx()
{
	uint64 x;
	asm volatile("csrrd %0, 0x10" : "=r"(x));
	return x;
}

static inline void w_tlbidx(uint64 x)
{
	asm volatile("csrwr %0, 0x10" : "+r"(x));
}

// TLBEHI（0x11）：TLB 条目的高位部分，包含虚拟页号和 ASID 等信息。
static inline uint64 r_tlbehi()
{
	uint64 x;
	asm volatile("csrrd %0, 0x11" : "=r"(x));
	return x;
}

static inline void w_tlbehi(uint64 x)
{
	asm volatile("csrwr %0, 0x11" : "+r"(x));
}

// TLBELO0（0x12）：偶数页 TLB 条目的低位部分。
static inline uint64 r_tlbelo0()
{
	uint64 x;
	asm volatile("csrrd %0, 0x12" : "=r"(x));
	return x;
}

static inline void w_tlbelo0(uint64 x)
{
	asm volatile("csrwr %0, 0x12" : "+r"(x));
}

// TLBELO1（0x13）：奇数页 TLB 条目的低位部分。
static inline uint64 r_tlbelo1()
{
	uint64 x;
	asm volatile("csrrd %0, 0x13" : "=r"(x));
	return x;
}

static inline void w_tlbelo1(uint64 x)
{
	asm volatile("csrwr %0, 0x13" : "+r"(x));
}

// TLBRENTRY（0x88）：TLB 重填异常入口物理地址。
static inline uint64 r_tlbrentry()
{
	uint64 x;
	asm volatile("csrrd %0, 0x88" : "=r"(x));
	return x;
}

static inline void w_tlbrentry(uint64 x)
{
	asm volatile("csrwr %0, 0x88" : "+r"(x));
}

// TLBRBADV（0x89）：触发 TLB 重填的错误虚拟地址。
static inline uint64 r_tlbrbadv()
{
	uint64 x;
	asm volatile("csrrd %0, 0x89" : "=r"(x));
	return x;
}

static inline void w_tlbrbadv(uint64 x)
{
	asm volatile("csrwr %0, 0x89" : "+r"(x));
}

// TLBRERA（0x8a）：TLB 重填异常返回地址和异常上下文标志。
static inline uint64 r_tlbrera()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8a" : "=r"(x));
	return x;
}

static inline void w_tlbrera(uint64 x)
{
	asm volatile("csrwr %0, 0x8a" : "+r"(x));
}

// TLBRSAVE（0x8b）：TLB 重填异常处理期间的软件临时保存寄存器。
static inline uint64 r_tlbrsave()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8b" : "=r"(x));
	return x;
}

static inline void w_tlbrsave(uint64 x)
{
	asm volatile("csrwr %0, 0x8b" : "+r"(x));
}

// TLBRELO0（0x8c）：TLB 重填异常上下文中的偶数页低位信息。
static inline uint64 r_tlbrelo0()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8c" : "=r"(x));
	return x;
}

static inline void w_tlbrelo0(uint64 x)
{
	asm volatile("csrwr %0, 0x8c" : "+r"(x));
}

// TLBRELO1（0x8d）：TLB 重填异常上下文中的奇数页低位信息。
static inline uint64 r_tlbrelo1()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8d" : "=r"(x));
	return x;
}

static inline void w_tlbrelo1(uint64 x)
{
	asm volatile("csrwr %0, 0x8d" : "+r"(x));
}

// TLBREHI（0x8e）：TLB 重填异常上下文中的高位信息。
static inline uint64 r_tlbrehi()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8e" : "=r"(x));
	return x;
}

static inline void w_tlbrehi(uint64 x)
{
	asm volatile("csrwr %0, 0x8e" : "+r"(x));
}

// TLBRPRMD（0x8f）：TLB 重填异常前的处理器模式信息。
static inline uint64 r_tlbrprmd()
{
	uint64 x;
	asm volatile("csrrd %0, 0x8f" : "=r"(x));
	return x;
}

static inline void w_tlbrprmd(uint64 x)
{
	asm volatile("csrwr %0, 0x8f" : "+r"(x));
}

// 刷新全部 TLB 项，语义对应 RISC-V 的 sfence.vma zero, zero。
// LoongArch 使用 INVTLB op=0；该操作不区分地址空间和虚拟地址。
static inline void sfence_vma()
{
	asm volatile("invtlb 0x0, $zero, $zero" ::: "memory");
}

// 使全部 TLB 项失效，保留此名称兼容现有 LoongArch 启动代码。
static inline void invtlb_all()
{
	sfence_vma();
}

// PGDL（0x19）：低半地址空间的页全局目录基地址。
static inline uint64 r_pgdl()
{
	uint64 x;
	asm volatile("csrrd %0, 0x19" : "=r"(x));
	return x;
}

static inline void w_pgdl(uint64 x)
{
	asm volatile("csrwr %0, 0x19" : "+r"(x));
}

// PGDH（0x1a）：高半地址空间的页全局目录基地址。
static inline uint64 r_pgdh()
{
	uint64 x;
	asm volatile("csrrd %0, 0x1a" : "=r"(x));
	return x;
}

static inline void w_pgdh(uint64 x)
{
	asm volatile("csrwr %0, 0x1a" : "+r"(x));
}

// PGD（0x1b）：根据 BADV/TLBRBADV 当前上下文选择的页全局目录基地址，只读。
static inline uint64 r_pgd()
{
	uint64 x;
	asm volatile("csrrd %0, 0x1b" : "=r"(x));
	return x;
}

// PWCL（0x1c）：低半地址空间的页表遍历控制信息。
static inline uint64 r_pwcl()
{
	uint64 x;
	asm volatile("csrrd %0, 0x1c" : "=r"(x));
	return x;
}

static inline void w_pwcl(uint64 x)
{
	asm volatile("csrwr %0, 0x1c" : "+r"(x));
}

// PWCH（0x1d）：高半地址空间的页表遍历控制信息。
static inline uint64 r_pwch()
{
	uint64 x;
	asm volatile("csrrd %0, 0x1d" : "=r"(x));
	return x;
}

static inline void w_pwch(uint64 x)
{
	asm volatile("csrwr %0, 0x1d" : "+r"(x));
}

// STLBPS（0x1e）：STLB 的统一页大小配置，PS 字段为页大小的 log2 值。
static inline uint64 r_stlbps()
{
	uint64 x;
	asm volatile("csrrd %0, 0x1e" : "=r"(x));
	return x;
}

static inline void w_stlbps(uint64 x)
{
	asm volatile("csrwr %0, 0x1e" : "+r"(x));
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
