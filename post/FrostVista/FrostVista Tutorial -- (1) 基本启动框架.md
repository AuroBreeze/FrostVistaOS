---
title: FrostVista Tutorial -- (1) 基本启动框架
date  : 2026-01-09
time  : 17:02:05
archive: FrostVista
categories: [OS, Kernel, C/C++, Tools]
summary: A beginner-friendly guide to building a minimal RISC-V OS boot framework with linker scripts, assembly startup code, UART output, and Makefile configuration.
---

# FrostVista的基本启动框架

或许大部分和人和我刚开始写OS一样会发懵，不知道该写啥，不知道应该去做什么。

但写 OS 最重要的迈出第一步，而不是在一开始就死磕复杂的启动原理。我们可以先搭建一个最简单的启动框架，让 OS 跑起来，再回过头去理解其中的细节。

所以说我们可以一开始写一个简单的`RISCV-OS`的启动框架，先用这个框架来实现启动测试再来回头理解这个框架。

# 使用的工具

要知道启动不同的架构的OS是需要点特殊的手段的，我们使用`qemu`来模拟`RISCV`架构，使用`riscv-gcc`进行编译，使用`ld`进行链接，使用`qemu-system-riscv64`进行模拟，使用`make`进行构建。

具体的工具如何下载和使用，我们就不详细介绍了，这些东西在浏览器或者AI里都可以很容易的找到。

# 启动框架

## LD 链接脚本

```ld
/* linker.ld — RISC-V virt bare metal kernel, entry: _start at 0x80000000 */
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
  RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
  /* QEMU virt DRAM start address */
  . = ORIGIN(RAM);

  /* .text.entry(_start) */
  .text :
  {
    . = ALIGN(4);
    *(.text.entry)           /*  _start  */
    *(.text .text.*)
    *(.rodata .rodata.*)
    . = ALIGN(0x1000);
    _divide = .;
  } > RAM

  . = ALIGN(16);
  .data :
  {
    _data_start = .;
    *(.sdata .sdata.* .data .data.*)
    _data_end = .;
  } > RAM

  . = ALIGN(16);
  .bss :
  {
    _bss_start = .;
    *(.sbss .sbss.* .bss .bss.* COMMON)
    _bss_end = .;
  } > RAM

  . = ALIGN(16);
  _stack_bottom = .;
  . = . + 0x4000;
  _stack_top = .;     /* alloc 16kb stack to kernel */

  . = ALIGN(0x1000);

  /* WARNING: High address mapping will cause address elevation*/
  _kernel_end = .;
}
```

关于LD链接脚本，需要知道一些简单的内容。

我们所编写的源代码，会编译成`.text`，`.data`，`.bss`，`.rodata`等段，这些`.text`，`.data`，`.bss`，`.rodata`段等，会分别被放到不同的内存区域中。

而我们需要自行组织这些区域，就需要使用LD链接脚本，将不同的段片放到不同的内存区域中。

在上面的组织中，我们的内存分配时这样的：

```
----------------------  <<--- 0x80000000
|      .text         |
|--------------------|
|      .data         |
|--------------------|
|      .bss          |
|--------------------|
|      .stack        |
|--------------------|
|      .kernel_end   |
----------------------  <<-- 0x80000000 + 128M
```

不过有些需要注意的时，在裸机程序运行的时候，需要自己组织`stack`，如果自己没有设置栈，那么程序会直接崩溃。

而在这里，OS刚刚开始启动的时候，可以简单的设置到我们的内核镜像的末尾，方便将他们作为一块连续的内存加载运行(也方便设置映射)。

而对于`LD`的链接脚本的使用，下面可以简单的介绍一下：

1. `ENTRY`: 这个是入口，这里我们配置入口为`_start`(一般是汇编语言编写的程序)，这个入口会作为程序入口，程序会从这里开始执行。
2. `MEMORY`：这个是内存的配置，这里我们配置了一个内存区域，名字叫`RAM`，起始地址是`0x80000000`，长度是`128M`，但是这个并不是限制内存只有`128MB`，只是告诉内核这`128MB`内存是**可用**的，这也对应了`QEMU virt`机器设置分配的内存大小。
3. `SECTIONS`：这个是段配置，这里我们配置了`.text`，`.data`，`.bss`，`.rodata`等段，并设置它们在`RAM`内存中的起始地址。在这里面类似*(.text .text.*)的作用是将`.text`段中的所有段都放到`.text`段中。


## 汇编程序

```S
.section .text.entry
.global _start
_start:
  # set stack pointer
  la sp, _stack_top

  la t0, _bss_start
  la t1, _bss_end
  # clear .bss section
1: 
  bgeu t0, t1, 2f
  sd zero, (t0)
  addi t0, t0, 8
  j 1b
2:
  call main
3:
  j 3b
```

汇编程序中，我们首先将`sp`指向`_stack_top`，然后初始化`.bss`段，然后调用`main`函数，然后进入一个死循环，这样我们的程序就启动了。

这里可能有点让人误解的地方，为什么`begu`的跳转地址使用`2f`，这个`f`是什么意思？为什么`j 1b`使用`1b`，这个`b`是什么意思？

在`GNU 汇编器(GAS)`的语法中，`b`的意思是向上跳转，`f`的意思是向下跳转，所以`j 1b`的意思是向上跳转到标签为`1`的行，`j 2f`的意思是向下跳转到标签为`2`的行。

我们也使用了`addi t0, t0, 8`，因为我们在上面使用的是`sd zero, (t0)`指令，也就是存储双字，我使用的是`RISCV64`，所以在上面的清空`.bss`段中，就是清理了`64位`，所以我们就使用`addi t0, t0, 8`来指向下一个地址进行覆写。

## C程序

```c
#define UART_ADDR 0x10000000
#define UART_DATA_REG 0

void putchar(char ch) {
  *(volatile char *)(UART_ADDR + UART_DATA_REG) = ch;
}

__attribute__((noreturn)) void main() {
  char *str = "Hello World!\n";
  while (*str) {
    putchar(*str++);
  }
  while (1) {}
}
```

我们可以先实现一个简单的`putchar`函数，来实现文字的输出。

> 需要注意的时，因为`qemu`会自动配置简单的`uart`，所以在我们的写OS的初期可以直接使用，用来检测OS是否可以启动。

## Makefile
```makefile
MAKEFLAGS += -j$(shell nproc)

CROSS  = riscv64-elf
CC     = $(CROSS)-gcc
DUMP   = $(CROSS)-objdump


CFLAGS = -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany \
         -nostdlib -nostartfiles -ffreestanding -O2 -Iinclude


LDFLAGS = -T linker.ld

SRCDIRS = . kernel/core kernel/driver kernel/arch/riscv kernel/mm kernel/tool

C_SRCS := $(foreach d,$(SRCDIRS),$(wildcard $(d)/*.c))
S_SRCS := $(foreach d,$(SRCDIRS),$(wildcard $(d)/*.S))

OBJS   := $(C_SRCS:.c=.o) $(S_SRCS:.S=.o)

QEMU      = qemu-system-riscv64
QEMUFLAGS = -machine virt -nographic -bios none -kernel kernel.elf

.PHONY: all clean run

all:
	$(MAKE) clean
	$(MAKE) -j$(nproc) kernel.elf
	$(MAKE) run

disasm: kernel.elf
	$(DUMP) -dS kernel.elf > disasm.txt

kernel.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

run: kernel.elf
	$(QEMU) $(QEMUFLAGS)

clean:
	rm -f $(OBJS) kernel.elf disasm.txt

```

我们就不详细介绍了，这里面主要是编译，链接，运行，清理等操作。
