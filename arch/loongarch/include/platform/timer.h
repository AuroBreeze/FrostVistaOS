#ifndef __LOONGARCH_PLATFORM_TIMER_H
#define __LOONGARCH_PLATFORM_TIMER_H

// QEMU LoongArch 恒定频率定时器为 100 MHz。
#define TIMER_FREQ 100000000ULL
// 与 RISC-V 当前 100 ms 调度周期保持一致，即每秒触发 10 次。
#define TIMER_HZ 10ULL

// TCFG 的低两位用于使能和周期模式，倒计时初值必须按 4 对齐。
#define TCFG_INITVAL_MASK 0xfffffffffffcULL

// TCFG.EN[0]：置 1 后启动倒计时。
#define TCFG_ENABLE (1 << 0)
// TCFG.Periodic[1]：置 1 后在归零时自动重新装载初值。
#define TCFG_PERIODIE (1 << 1)
// 将以恒定频率时钟 tick 表示的周期编码到 TCFG.InitVal。
#define TCFG_INITVAL(ticks) ((uint64) (ticks) & TCFG_INITVAL_MASK)

void timer_init();

#endif
