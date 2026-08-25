#include "platform/timer.h"
#include "asm/loongarch.h"
#include "kernel/types.h"

void timer_init()
{
	// 将目标中断频率换算为一次倒计时所需的恒定频率时钟 tick。
	uint64 ticks = TIMER_FREQ / TIMER_HZ;

	uint64 ecfg = r_ecfg();
	uint64 crmd = r_crmd();
	// ECFG.LIE[11] 对应处理器核内的定时器中断。
	uint64 ecfg_enable = 1 << 11;
	// CRMD.IE[2] 是 PLV0 的全局中断使能位。
	uint64 crmd_ie = 1 << 2;

	w_ecfg(ecfg | ecfg_enable);
	w_crmd(crmd | crmd_ie);

	// 以周期模式启动定时器，归零后自动重装载 ticks。
	w_tcfg(TCFG_INITVAL(ticks) | TCFG_PERIODIE | TCFG_ENABLE);
}
