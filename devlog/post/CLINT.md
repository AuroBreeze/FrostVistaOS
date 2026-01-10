Author: AuroBreeze
Date  : 2025-12-25
Time  : 16:51:18

title: CLINT的SBI调用链实现

---

# CLINT的SBI调用链实现

本文主要讲述我在实现SBI调用链实现CLINT定时时遇到的问题，以及如何解决。

需要使用到以下文件:
- RISCV-SBI-Spec.pdf
- RISCV_ABI.pdf
- RISCV-Privileged.pdf

涉及到的章节：
- SBI: 
    - Chapter 1 Introduction
- ABI: 
    - Chapter 1.1 Integer Register Convention
- Privileged: 
    - Chapter 3.1.5. Hart ID (mhartid) Register
    - Chapter 3.1.6.1 Privilege and Global Interrupt-Enable Stack in mstatus register
    - Chapter 3.1.8. Machine Trap Delegation (medeleg and mideleg) Registers
    - Chapter 3.1.9. Machine Interrupt (mip and mie) Registers
    - Chapter 3.1.13. Machine Scratch (mscratch) Register
    - Chapter 3.1.15. Machine Cause (mcause) Register
    - Chapter 3.2.1. Machine Timer (mtime and mtimecmp) Registers

[TOC]

## 1. 引言

本文默认读者已经了解简单的RISC-V架构，以及RISC-V SBI和OpenSBI。

在前面实现OS内核的时候，我选择的是-bios none的启动方式，也就是意味着无法使用OpenSBI，所以我的打算就是自行实现SBI调用链，尽量往工程实践中靠拢。

## 2. 简要

在阅读RISCV-SBI-Spec.pdf，简介中简单描述了SBI的调用

```
|    User Mode          |
         |
         |     System Call
         ↓
|    Supervisor Mode    |
         |
         |     SBI Call
         ↓
|    Machine Mode       |
```

我们可以看到SBI的调用是从S模式进入到M模式，然后实现SBI的功能后，再返回到S模式。

>> 所以，在这里就会涉及到特权级切换。

同时我们也要知道，在RISCV中的CLINT是一个时钟中断控制器，当他发生时钟中断时，会发送`Machine Timer Interrupt`，让我们的OS进入到M模式(在没有进行中断委托的时候)。

>> 所以，因为CLINT的时钟中断，我们也就会涉及到中断，委托中断的设置。

## 3. 总体思路

首先是中断，及中断委托的问题。

我们在先进入mstart函数时，处于M模式下，通常这个情况下，会进行基础的配置和中断委托。

涉及到CLINT中的`Machine Timer Interrupt`，所以在开始的配置过程中，我们要将M模式下的`Machine Timer Interrupt`委托到S模式。

同时要将M模式下的全局中断打开，也就是`mie`中的信息。

在`RISCV-Privileged`的`mie`中有比较详细的介绍，我们要开启`MTIE`中断使能位，不过需要保证的是，在开启`mstatus`中的`MIE`中断使能位时，一定要有处理`Machine timer Interrupt`的函数，也就是需要M态的中断处理入口及处理函数，否则的话，OS会直接因为中断无法处理，一直陷入中断，导致OS直接挂起。

当然，其中的`Supervisor timer interrupt`同时也需要委托到S模式，因为M模式下会处理`Machine timer Interrupt`，我们想要实现到S模式下的中断，就需要去变通一下，也就是在M模式处理`Machine timer interrupt`时，将`MIP`中的`STIP`中断挂起位置1，使其可以在S模式下，可以响应被委托的`Supervisor timer interrupt`中断。

所以简化一点:


0. 设置`sie.STIE`,使其响应`Supervisor timer interrupt`，开启`sstatus.SIE`

1. 通过SBI设置下一次的中断时间。
2. 时间到达后，M模式响应`Machine timer interrupt`，处理中断，并将下一次的响应时间设置到无限远(等待设置下一次响应时间)，并置`mip.STIP`让M模式中断后，让S模式响应`Supervisor timer interrupt`。
3. S模式响应`Supervisor timer interrupt`，在处理函数中，通过SBI设置下一次的中断时间
4. SBI通过`ecall`进入M模式中断处理函数，处理`Environment call from S-mode`异常，通过传入的参数，设置下一次的响应时间，并清除`mip.STIP`中断挂起位，防止S模式中断死循环。
5. 返回2，进行循环。


## 4. 需要注意的细节

- x模式下，xstataus.xie，xie，xip的区别及作用。

首先，先说xie和xip的区别，首先xie是控制中断使能位，xip控制中断挂起位，xie控制对应模式是否想管，xip控制对应模式是否触发中断。而xstatus.mie管理是否启动xie寄存器。

所以可以简化为

```
if(xstatus.mie && (mie & mip))
```

还有一些细节，就需要去看`RISCV-Privileged`中的`mie`和`mip`的章节。

- CLINT的配置

CLINT的mie，mstatus的配置要及时在M模式下进行配置，否则在S模式下配置，而且没有中断委托和处理的话，会导致系统直接挂起。


## 5. 具体实现


```
void pre_timerinit() {
  uint64 mie = r_mie();
  mie &= ~(MIE_MSIE | MIE_MEIE);
  mie |= MIE_MTIE;
  w_mie(mie);

  //
  w_mcounteren(0xffff);
}

void timerinit() {
  kprintf("Enable time interrupts...\n");

  w_sie(r_sie() | SIE_STIE);
  sbi_set_timer(r_time() + 1000000);
  w_sstatus(r_sstatus() | SSTATUS_SIE);
  kprintf("Enabled\n");
}
```

`pre_tiemrinit`是需要在M模式下进行配置，而且需要先实现`w_stvec`，并及时实现处理`Machine timer interrupt`，否则会因为设置完`mie`后，CLINT触发`Machine timer interrupt`没有处理函数导致OS挂起


而`w_mcounterrn`的配置就是为了实现在S模式下访问`time`寄存器，可以直接去看`RISCV-Privilege`中相关的内容


`tiemrinit`放在S模式下及时初始化即可。


```
void s_trap_handler(void) {
  uint64 sc = r_scause();
  uint64 epc = r_sepc();
  uint64 tval = r_stval();
  // kprintf("sc: %p\n", (void *)sc);

  if ((sc >> 63) == 1) {
    uint64 cause = sc & ((1ULL << 63) - 1);
    // determine if it's a timer interrupt
    // wo notify S state by setting SIP_SSIP (Software Interrupt Pending)
    if (cause == 5) {
      // timer interrupt
      // set timer for next interrupt
      sbi_set_timer(r_time() + 1000000);
      kprintf("Tick\n");
      return;
    }
  }
}
void m_trap(uint64 mcause, uint64 mepc, uint64 *regs) {
  // Check whether the most significant bit is ahn exception or an interrupt
  int is_interrupt = (mcause >> 63) & 1;

  uint64 code = mcause & ((1ULL << 63) - 1);
  if (is_interrupt) {
    if (code == 7) {
      uint64 *mtimecmp = (uint64 *)CLINT_MTIMECMP(r_mhartid());
      *mtimecmp = -1ULL; // Set the distance to infinity

      w_mip(r_mip() |
            MIP_STIP); // Key: Set STIP to allow S to receive scause = 5 }
    return;
  } else {
    if (code == 9) {         // ecall from S
      uint64 eid = regs[16]; // a7
      uint64 fid = regs[15]; // a6
      uint64 arg0 = regs[9]; // a0 = stime_value

      if ((eid == SBI_EID_TIME && fid == SBI_FID_SET_TIMER) ||
          (eid == SBI_EID_LEGACY_SET_TIMER)) {

        uint64 *mtimecmp = (uint64 *)CLINT_MTIMECMP(r_mhartid());
        *mtimecmp = arg0;

        regs[9] = SBI_SUCCESS; // a0
        regs[10] = 0;          // a1
      } else {
        regs[9] = -2; // SBI_ERR_NOT_SUPPORTED
        regs[10] = 0;
      }

      w_mip(r_mip() & ~MIP_STIP);

      w_mepc(mepc + 4);
      return;
    } else {
      while (1)
        ;
    }
  }
}
```

这就是M模式下处理中断函数和S模式下处理中断函数。

路径就是：

M模式下编号为7的`machine timer interrupt`中断，S模式下的编号为5的`Supervisor timer interrupt`中断，sbi触发的编号为9的`Environment call from S-mode`异常，然后循环

记住不要把编号为9的异常委托到S模式下处理，否则M模式无法接收到中断，而导致M无法处理SBI的ecall调用


