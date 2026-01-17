---
title: FrostVista Tutorial -- (2) UART驱动编写
date  : 2026-01-10
time  : 14:29:05
archive: FrostVista
categories: [OS, Kernel, C/C++, driver]
summary: 
---

# FrostVista Tutorial -- (2) UART驱动编写

我们在上一章中已经说明完如何进入OS并启动了，现在我们要做的事情就是要让OS可以开口说话，也就是`UART`驱动的编写。

要知道，在正常的OS中，是没人帮你配置好简单的`UART`的，所以，我们需要自己编写一个`UART`驱动，来完成这个功能。

而且只有实现了`UART`驱动，才能让OS开口说话，我们也可以正常的打印错误日志，消息等，否则的话，OS发生了什么是根本不知道的。

# 准备

要想要实现`UART`驱动，我们需要知道`UART`的寄存器地址，以及`UART`的寄存器结构，以及在`qemu`中，`UART`的配置地址。

我们在这里要使用`uart16550`驱动手册，以及导出的`qemu`的**设备树**来获取`UART`的配置地址。

具体导出**设备树**我们就不进行具体的讲解了，我们直接开始进行`UART`驱动的编写。

导出**设备树**的命令为：
```
qemu-system-riscv64 -machine virt -m 128M -smp 1 -nographic -machine dumpdtb=virt.dtb

dtc -I dtb -O dts -o virt.dts virt.dtb
```

> `uart` 驱动的名称是 `ns16550`
> `uart` 寄存器的地址是 `0x10000000`

# 编写驱动

对于我们刚开始起步的OS来说，没有必要直接就使用`中断处理`的方式来进行接收和发送消息，而是使用`轮询`的方式来处理，这样比较方便，也无需去写中断处理程序。

所以首先阅读`uart`的驱动手册，找到`uart`的寄存器结构，以及`uart`的寄存器的地址，然后编写驱动程序。

## uart必要宏定义

[参阅uart.h](../../uart.h)

这是基于`uart16550`的驱动的定义，不过`MSR`对我们写OS来说是没有什么必要使用的，具体可以自行了解。

不过，在驱动手册中，也有一些需要注意的点。
1. 当开启**除数锁存器**时，会占用偏移为`0`和`1`这两个寄存器，所以在设置完**除数锁存器**后，需要及时关闭除数锁存器。
2. 在写**除数锁存器**时，要先写高位，再写低位，在驱动手册中有明确的注明: **The internal counter starts to work when the LSB of DL is written, so when setting the divisor, write the MSB first and the LSB last.**

而对于其他位置的定义，都可以在`uart16550`驱动手册中查找的到，所以具体的细节我们就不再具体的讲解。

在`uart.h`中，我们还使用了`Red`, `ReadReg`, `WriteReg`这三个宏定义，分别用于设置寄存器地址，读取寄存器，写入寄存器。

要知道的事情是，在写OS时，与硬件的交互是通过内存来进行的，想要接收数据，就去对应的硬件地址去读，想写，发送数据，就去对应的硬件地址去写。

所以`Reg`的作用就是返回硬件地址，通过解引用后进行读写操作。

[参阅uart.h](../../uart.h#51)

## uart初始化

在`uart16550`驱动手册中的5.1章节中，介绍了uart的初始化过程，这里我们按照这个流程进行初始化。

```
Upon reset the core performs the following tasks:
1. The receiver and transmitter FIFOs are cleared.
2. The receiver and transmitter shift registers are cleared
3. The Divisor Latch register is set to 0.
4. The Line Control Register is set to communication of 8 bits of data, no parity, 1 stop bit.
5. All interrupts are disabled in the Interrupt Enable Register.
For proper operation, perform the following:
1. Set the Line Control Register to the desired line control parameters. Set bit 7 to ‘1’ to allow access to the Divisor
Latches.
2. Set the Divisor Latches, MSB first, LSB next.
3. Set bit 7 of LCR to ‘0’ to disable access to Divisor Latches. At this time the transmission engine starts working
and data can be sent and received.
4. Set the FIFO trigger level. Generally, higher trigger level values produce less interrupt to the system, so setting
it to 14 bytes is recommended if the system responds fast enough.
5. Enable desired interrupts by setting appropriate bits in the Interrupt Enable register.
```

[参阅uart.c](../../uart.c#3)

不过，因为我们最开始使用的是**轮询**的方式，所以，我们不需要进行**中断**的初始化操作，我们删除部分初始化的流程，只保留最基本的初始化流程。

这样就实现了`UART`的**轮询**的初始化。

## uart发送数据

[参阅uart.c](../../uart.c#15)

`uart`的**发送数据**流程如下：
1. 检查**发送缓冲区**是否为空，如果为空，则返回。
2. 检查**发送缓冲区**是否为满，如果为满，则等待。
3. 发送数据。

在`uart`的**发送数据**流程中，`LSR`的宏定义不是用来进行设置的，是用来查看和比对的，也就是查看是否为满，比如`UART_LSR_TX_EMPTY`，再根据我们前面写好的发送的宏定义，即可完成发送数据的函数。


