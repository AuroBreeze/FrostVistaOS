---
title: FrostVista: Minimal U-mode implementation
date  : 2026-03-17 
time  : 13:49
archive: FrostVista
categories: [OS, Kernel, C/C++, FrostVista, RISC-V]
summary:
---

# FrostVista: Minimal U-mode implementation

本文主要讲述在实现简单的U-mode时可能遇到的问题，以及一些疑问。

本文主要讲解思路而非代码，本文中的代码也仅仅是我实现所用的代码，仅供参考。

需要使用到以下文件:
- RISCV_ABI.pdf
- RISCV-Privileged.pdf

涉及到的章节：
- ABI: 
    - Chapter 1.1 Integer Register Convention
- Privileged: 
    - Chapter 12.1.6 Supervisor Scratch (sscratch) Register
    - Chapter 12.1.7 Supervisor Exception Program Counter (sepc) Register
    - Chapter 12.1.1 Supervisor Status (sstatus) Register

[TOC]

## 1. 引言

本文默认读者已经了解简单的RISC-V架构，以及各个特权级的特性。

## Preheat

先来回想一下如何从M模式进入S模式，步骤很简单：

1. 设置物理内存保护的访问权限(pmpaddr0 and pmpcfg0)
2. 设置页表(satp) 并 刷新(sfence.vma)
3. 设置中断向量(mstvec)
4. 设置mstatus.MPP为S模式
5. 设置中断委托(mideleg and medeleg)
6. 写入要进入S模式要执行函数(mepc) 

而从S模式进入U模式也是照猫画虎的过程，不过要注意变通

1. 设置页表(satp)
2. 设置中断向量(stvec)
3. 设置sstatus.SPP为U模式
4. 写入要进入U模式要执行函数(sepc)

似乎比M模式进入S模式要简单，实则不然，这个过程还需要考虑到寄存器的保存以及进程的设计等等。

> 你可能想问为什么M模式进入S模式没有涉及到寄存器的保存，而S模式进入U模式需要保存寄存器的值，那么这是为什么呢？
> 简单来说，这是关于“在何处运行”和“断点”的问题，我们在M模式下，拥有最高的权限，而进入S模式下是为了降低权限，并在此处运行，进入S模式后，我们的代码大多是不在M模式下运行的，是没有必要回去的，而保存寄存器的作用就是为了保存此刻的状况以便回去继续执行，而我们没有回M模式的必要也就不需要保存寄存器的值。
> 但是，我们的代码大多都是在U模式下运行，随时随地都有可能会被中断所打断进入S模式，而为了能够在处理完中断后返回U模式继续执行代码，所以就不得不保存进入中断后要被覆盖的寄存器的值。


所以，既然麻烦，最好是先通过一个测试来直接进入U模式，来测试，测试通过后，再来改进。

所以我们可以先选择实现这样一个简单的测试:

```c
uint32 user_code[2]={
      0x00000073, // ecall
      0x0000006f  // j .
}
```
## start

### uservec.S

要知道的是，我们要构造上面的程序的话，是要进行中断处理的，回想在S模式下，我们是如何实现中断处理的？

通过使用 **.S** 文件，来保存寄存器并进入S模式下的中断处理程序。我们也要先写一个汇编文件来保存寄存器

```
  .section .text
  .global uservec
  .align 2
uservec:
  # 1. exchange stack. After this, sp=kernel stack, sscratch=user stack
  csrrw sp, sscratch, sp

  # 2. allocate trapframe
  addi sp, sp, -256

  # 3. save t0, because we need to use it to save user sp
  sd t0, 32(sp)

  # 4. save user sp to trapframe
  csrr t0, sscratch
  sd t0, 8(sp)

  # 5. save the registers.
  sd ra, 0(sp)
  sd gp, 16(sp)
  ... 
  sd t6, 240(sp)

  call usertrap

# ==========================================

.global userret
userret:
  mv sp, a0

  addi t0, sp, 256
  csrw sscratch, t0

  # start restore except a0 and sp
  ld ra, 0(sp)
  ld gp, 16(sp)
  ...
  ld t6, 240(sp)

  # first restore a0, then restore sp that points to trapframe
  ld a0, 72(sp)
  
  # finally restore sp
  ld sp, 8(sp)

  sret
```

在这段汇编代码中，`uservec` 将数据全部保存到`trapframe` 中(这个我们下一步讲), 特别需要注意顺序的问题，否则很容易导致覆盖寄存器的值。

而在`userret(struct trapframe*)` 中将数据全部恢复并使用`sret` 返回U模式。

### struct trapframe

```c
struct trapframe{
  uint64 ra;
  uint64 sp;
  ...
  uint64 t6;

  // other value
  uint64 epc;
}
```

这段代码的作用也很简单，就是专门保存寄存器中的数据，当然为了偷懒，我也加了一个`epc` 用来写入`sepc`用。

有一个重要的点， **sizeof(trapframe) = 256** 这也就是为什么我们在上面的sp中使用`add sp, sp, -256/256` 了

要是`trapframe` 的大小变化的话，还是需要更改uservec中的关于`sp` 的增加或减少的值的。

### usertrap()

也就像我们前面说的一样，要先实现中断处理程序，

我们要考虑什么才能写出这个中断处理程序呢？

从零开始可能是个伪命题，如果没有借鉴，很难，你是天才，那另当别论，hhh。

首先，我们处理的就是中断，要不然怎么叫中断处理程序，hhh。判断好是中断还是异常，并分别对他们进行处理。

> 需要注意的是，高级别的中断会打断低级别的运行，如果设置时钟中断的S模式下的委托，我们在U模式下运行时，也会被时钟中断所打断，进入uservec，所以也要在uservec中处理这个时钟中断。

毕竟我们要先写一个`ecall`的中断，所以要在中断处理程序中针对ecall来进行处理，比如打印个什么来测试

正如我们上面说的，高级别的中断会打断低级别的中断，如果我们在这个时候，在S模式下被M模式下的中断打断了，会发生什么？

很简单，会再次通过`stvec`进入uservec，但是没有对应的处理程序，导致出现bug。

所以，要尽量早的设置`stvec`来设置S模式下的中断处理程序。

如果我们仔细看RISCV-Privileged中sstatus的内容，你会发现，`SPP` 存放的是在进入中断处理程序之前的特权级(The SPP bit indicates the privilege level at which a hart was executing before entering supervisor mode.)，所以我们也可以检查一下这个位。

首先，必须是检查`sstatus` 中的`SPP` 位是否为**1**, 如果不为`1`则证明这个中断不是从U模式下进入的，这就是BUG了。

至于怎么获取中断信息等，我们就不仔细说明了。

```c
void usertrap(void) {
  // LOG_TRACE("usertrap");
  if ((r_sstatus() & SSTATUS_U_SPP) != 0) {
    panic("usertrap: not from user mode");
  }
  // write kernel trap vector that handles new interrupts in S mode
  trapinit();

  uint64 sp = r_sp();
  struct trapframe *tf =
      (struct trapframe *)(PGROUNDUP(sp) - sizeof(struct trapframe));

  mytrapframe = tf;

  tf->epc = (uint64)r_sepc();

  uint64 cause = r_scause();

  if ((cause >> 63) == 1) {
    uint64 exception_code = cause & ((1ULL << 63) - 1);
    if (exception_code == E_S_TIMER_INTERRUPT) {
      sbi_set_timer(r_time() + 1000000);
      yield();
      LOG_TRACE("Tick in U-mode");
    } else {
      LOG_ERROR("Unexpected interrupt in U-mode, code: %d", exception_code);
    }
  } else {
    if (cause == 8) {
      LOG_INFO("Target Eliminated: Successfully executed 'ecall' in U-mode!");
      syscall();
      tf->epc += 4;

      // test can the value be passed normally
      LOG_DEBUG("tf-a2: %d", tf->a2);
    } else {
      LOG_ERROR("Unexpected trap, cause: %d", cause);
      while (1)
        ;
    }
  }

  usertrapret();
}
```

### usertrapret

处理完中断，我们还要返回到U模式继续执行我们的代码，我们要怎么返回呢？

或者换个问法，我们要怎么从S模式进入U模式呢？

是不是很耳熟？没错就是我们上面讲的，不过稍微有点变化

> 上文
> 1. 设置页表(satp)
> 2. 设置中断向量(stvec)
> 3. 设置sstatus.SPP为U模式
> 4. 写入要进入U模式要执行函数(sepc)

设置页表这个东西，我们要放到后面讲，现在不会用。

我们的`stvec` 在上文我们改过，改成了`kernelvec`，这是要处理S模式的中断，我们要返回的U模式，所以我们要写回`uservec`

> 有没有发现问题，如果我们这个时候把`stvec` 改成了`uservec`的话，会发生什么问题？这个时候发生个S模式的中断会怎么样？ 爆炸了！hhh

所以在覆写`stvec`时，要先关中断，把`sstatus`中的`SIE`的中断关闭，不接受中断，这样就没什么问题了，不过你也可能会问，那我什么时候把这个`SIE`打开？可以去看看`SPIE`，我们不再多说。

然后就是，设置`sstatus.SPP`

`sepc` 要注意要进行增加，`sepc`保存的只是触发中断时的`pc`值，要是不进行增加就会一直在触发中断的位置一直触发。

```c
void usertrapret(void) {
  // LOG_TRACE("usertrapret");
  // Set SIP that turns off all interrupts
  intr_off();

  // write kernel trap vector
  extern void uservec(void);
  w_stvec((uint64)uservec);

  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_U_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE;   // enable interrupts in user mode
  w_sstatus(x);

  w_sepc(mytrapframe->epc);

  extern void userret(struct trapframe *);
  userret(mytrapframe);
}
```

### create_user_pagetable()

创建用户页表，我们也知道页表的重要性，所有的地址信息都保存到这上面。

我采用的是类linux的共享页表的机制，也就是共享内核页表，所以在实现程度上并没有跳板页那样的实现难度，当然，后面内核页表的同步可能也是一个问题。

> 有没有想过，共享页表机制在触发中断的时候发生了什么？为什么会涉及到切换页表这个东西，为什么不用像xv6那样切换页表。

在中断发生的瞬间

1. 特权级从U模式提升到S模式
2. `pc` 保存到 `sepc`
3. 将`pc` 设置为 `stvec`

所以，对于我们的共享页表的机制，即使特权级从U提升到S模式，因为我们的内核页表一直在用户页表中，所以我们并不需要什么动作。

不过需要注意的是，为什么使用跳板页的OS要切换页表？为什么我们的要切换到内核页表？

答案很简单，我们的中断处理程序是映射到内核页表的，想一想，除了我们准备要实现的这个用户程序马上要使用`kvmmap` 映射页表外，其他地方有用到`kvmmap` 吗？也就是说，我们上面写的代码，早就映射到内核页表了！！

那么简单再说一下跳板页，因为中断发生的瞬间，特权级提升了，并且进入了`stvec`指向的地址，通常这种情况下就是先保存寄存器的值，然后在汇编中切换`satp`的值，并刷新，就正式使用内核页表了，之后再进入中断处理程序。

```c
static pagetable_t create_user_pagetable() {
  pagetable_t user_pagetable = (pagetable_t)kalloc();
  if (user_pagetable == 0) {
    panic("Failed to allocate memory");
  }

  memset(user_pagetable, 0, PGSIZE);

  // mapping kernel pagetable
  for (int i = 256; i < 512; i++) {
    extern pagetable_t kernel_table;
    user_pagetable[i] = kernel_table[i];
  }

  return user_pagetable;
}
```

### user_init()

好了，到现在就是准备正式的准备U模式的程序了。

U模式的程序的启动流程和我们正常在S模式下配置OS的启动的区别没有太多区别，无非就是，获取内存，存放程序，配置地址映射等。

当然，有些地方还是需要注意一下，比如，程序的运行需要栈，这个也需要单独的分配内存，而且栈的增长是往低地址增长的。

所以说按照步骤，我们先通过两个`kalloc` 来获取两个内存，一个用来存放程序，也就是我们在文章开头所写的

```c
uint64 user_code[2]={
  0x00000073, // ecall
  0x0000006f  // j .
}
```

另一个作为程序的栈，当然申请完内存后要再加`PGSIZE`。

目前的程序只是我们写好的一个数组，程序当然不可能以这种形式来进行运行。

程序的编译完后都是二进制的形式，我们用的这个极简的测试代码就是已经编译好的，我们需要做的就是把它加载到内存里面，然后运行。

我们如何让这个程序加载到内存里面？

很简单就是通过memcpy直接将数组中的数据，复制到我们申请的内存空间中。

然后我们配置地址映射，赋予这个程序的用户态权限，可读，可写，可执行，以及有效。

栈的地址映射同理。不过注意可能会出现重映射到问题，可以把栈的映射位置移到靠后面一点。

然后要设置`epc`，我上文提到过我为了偷懒把epc，放到了`trapframe`中，所以我可以直接写入，然后设置sp

现在覆写pc指向你映射的区域就可以实现执行了，当然具体代码就不再展示。

> 代码可能会有点不适配，本文主要使用调度等实现程序的运行。

## schedule

调度，什么是调度？怎么实现调度？

调度就是不要一直让一个程序一直占用CPU，要不断的调度其他程序进入运行。

那要怎么实现调度呢？

首先要处理的还是我们要调度什么？

当然是进程，也是对一个程序的包装

为什么要进行包装？

如果不包装，程序直接运行了，你知道他在什么地方运行吗？你知道他的栈在哪里吗？你知道他运行了多久了吗？(当然，程序少你自然可以记住，但是程序很多)

所以进程就会保存很多这个程序的信息，包括上面我们提到的信息，还有一些有的没的，hhh。

那调度呢？怎么实现两个进程之间的切换？

换句话说，你既然运行了一个程序，你怎么停下这个程序，用类似的方法再运行一个程序？

下面我们来解决这个问题。

> 此代码非最小U模式的代码，只为适配调度模式

```c
void user_init() {
  LOG_TRACE("Initializing user process");
  struct Process *p = alloc_process();
  if (p == 0) {
    panic("Failed to allocate process");
  }

  uint64 user_code_table = (uint64)kalloc();
  if (user_code_table == 0) {
    panic("Failed to allocate memory");
  }
  uint64 user_stack = (uint64)kalloc();
  if (user_stack == 0) {
    panic("Failed to allocate memory");
  }
  uint32 user_code[4] = {
      0x00100893, // addi a7, zero, 1
      0x06300513, // addi a0, zero, 99
      0x00000073, // ecall
      0x0000006f  // j .
  };
  memcpy((uint64 *)user_code_table, user_code, 16);

  kvmmap(p->pagetable, 0x0, (uint64)ADR2LOW(user_code_table), PGSIZE,
         PTE_U | PTE_R | PTE_W | PTE_X | PTE_V);
  uint64 user_stack_va = 0x40000;
  kvmmap(p->pagetable, (uint64)user_stack_va, (uint64)ADR2LOW(user_stack),
         PGSIZE, PTE_U | PTE_R | PTE_W | PTE_V);

  uint64 user_stack_top = (uint64)user_stack_va + PGSIZE;
  p->trapframe->sp = user_stack_top;
  p->trapframe->epc = 0x0;

  // Test whether it can be stored in order normally
  p->trapframe->a2 = 666;

  p->state = RUNNABLE;
  LOG_TRACE("User process initialized");
}
```

### PCB

我们这里打算使用调度进入运行。

调度什么？调度进程，我们的进程还没有定义，进程应该定义什么内容？

或者说，我们应该记录一个程序的什么东西？

他的栈，他的名字，ID，使用的寄存器(还要区分调度和中断)，页表。

> 为什么保存使用的寄存器要区分调度和中断？直接全保存寄存器用一个结构体不行吗？
> 因为中断是随时随地，突然的发生的，而调度是有规律的(当然，也不能这么讲)，进程调度时，会由编译器保证其他的寄存器的数据是无用的(有用也会提前保存)，也就是ABI的规定，所以我们只需要保存必要的寄存器中的数据即可，而中断就需要全部保存了。

让我们想想，假设一个什么记录都没有的程序在OS上运行，你为了找到他，你会想要他的什么信息？


```c
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  ...
  uint64 s11;
};

struct Process {
  enum proc_state state;
  int pid;       // Process ID
  char name[16]; // Process name

  uint64 kstack;               // Kernel stack pointer
  pagetable_t pagetable;       // Page tabl
  struct context *context;     // Kernel context
  struct trapframe *trapframe; // User trap frame
};
```

### swtch

实现调度，最重要的当然是在不同的程序之间的切换，而要切换程序，我们要保存什么？首先当时是上文我们说的，保存寄存器。

详细请查阅：RISCV-ABI.pdf

```asm
.globl swtch
swtch:
        sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        ...
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ...
        ld s11, 104(a1)
        
        ret
```

很简单对吧

> 记住这个`ret`

### alloc_process

接下来我们实现对运行程序的包装的获取，也就是进程的获取，以及初始化

要做的也很简单，找到可用的进程，获取，并初始化他的状态，初始化栈，页表等等

```c
struct Process *alloc_process(void) {
  struct Process *p;
  for (p = proc; p < &proc[64]; p++) {
    if (p->state == UNUSED) {
      p->state = USED;
      p->kstack = (uint64)kalloc();
      p->pagetable = create_user_pagetable();

      // NOTE:
      // Position the trapframe above the stack, that is, at a lower address
      // in order to store data in the tramframe
      p->trapframe = (struct trapframe *)(p->kstack - sizeof(struct trapframe));

      extern void usertrapret(void);
      // NOTE: p->context must be allocated in the kernel otherwise it will be
      // panic
      p->context = (struct context *)kalloc();
      p->context->ra = (uint64)usertrapret;

      // NOTE:
      // Point sp to a location not used by the trapframe
      p->context->sp = p->kstack + PGSIZE - sizeof(struct trapframe);
      return p;
    }
  }
  return 0;
}
```


不知道，你还记不记得我们前面说的`trapframe` 在这里出现的有一点奇怪

这是我们前文的代码

```c
  uint64 sp = r_sp();
  struct trapframe *tf =
      (struct trapframe *)(PGROUNDUP(sp) - sizeof(struct trapframe));
```

估计当时你看到这里会感觉很奇怪吧，我们在这里设置的sp和trapframe的地址也很奇怪，因为都是为了这一小段代码服务的(当然这也写有问题，后续可以自行改进)

因为我们的`usertrap`是无形参的，为了使用`trapframe`中的数据，我们通过设置特殊地址的方式寻址得到，这是怎么实现的？

我们的`sscratch`设置为栈顶，也就是`p->stack + PGSIZE`，然后将`p->trapframe`指向低`sizeof(struct trapframe)`字节的地址，`sp`也改为这个地址，因为上面的栈已经被`trapframe`使用了，所以栈只能在trapframe下面增长(栈是往下增长的)。

因为我们在`uservec`中会先`addi sp,sp,-256`往下增长，而`trapframe`读数据却是往上加，所以这样，可以通过地址对齐获取`trapframe`并使用里面的信息。

```
p->kstack (Low Address)
  +---------------------------+ <--- 栈底 (Low Address)
  |      (空闲空间)           |
  +---------------------------+ <--- p->trapframe 指针指向这里
  |                           |
  |      struct trapframe     |  <--- 用于存放用户态现场 (U-mode registers)
  |    (epc, ra, sp, ...)     |
  |                           |
  +---------------------------+ <--- p->context->sp (指向 trapframe 底部下方)
  |      (空闲/运行空间)       |
  |      Kernel Stack         |
  |                           |
  +---------------------------+ <--- 栈顶 (p->kstack + PGSIZE, High Address)
```



### schedule

我们是如何开始第一个程序的呢？分配空间，栈等，现在我们要切换页表，设置空闲栈顶。

我们找到待运行的程序，在`sscratch`中设置栈顶，因为我们在`uservec`会首先用`sscratch`和`sp`交换数据，然后减256用来存寄存器的信息，当然，我们通过地址对齐，使其存放到了我们的`trapframe`


```c
void scheduler(void) {
  struct Process *p;
  extern void swtch(struct context * old, struct context * new);

  for (;;) {
    for (p = proc; p < &proc[64]; p++) {
      if (p->state == RUNNABLE) {
        p->state = RUNNING;
        current_proc = p;

        extern struct trapframe *mytrapframe;
        mytrapframe = p->trapframe;

        // NOTE:
        // Because in uservec, addi sp, sp, -256 is first used, uservec can
        // properly align with the trapframe and store data into it.
        w_sscratch(p->kstack + PGSIZE);

        w_satp(MAKE_SATP(ADR2LOW((uint64)p->pagetable)));
        sfence_vma();

        swtch(&scheduler_context, p->context);

        current_proc = 0;
        extern pagetable_t kernel_table;
        w_satp(MAKE_SATP(kernel_table));
        sfence_vma();

        LOG_TRACE("Switched back to kernel");
      }
    }
  }
  LOG_TRACE("Scheduler done");
}
```

## More detailed information

关于为什么在`alloc_process`中的`ra = (uint64)usertrapret`？

在这里为什么会这样设计？

我们首先知道的是，`ra`的作用就是作为返回地址使用的，而在我们的设计中，也就是在调度中，通过`swtch`进行两个进程之间的切换，还记得我前面写的记住的`ret`的返回吗？`swtch`中的`ret`会进行返回，而返回到哪里就是由`ra`所决定的。

为什么我们会选择返回到`usertrapret`中？还记得前面我问过，程序的启动吗？

> 1. 设置页表(satp)
> 2. 设置中断向量(stvec)
> 3. 设置sstatus.SPP为U模式
> 4. 写入要进入U模式要执行函数(sepc)

我们的设置页表已经在调度里面实现了，而`usertrapret`正好剩下的所有功能全部都实现了，这就是个完美的从S模式进入U模式的跳板，所以....


## end

本文就讲到这里了，本文主要还是讲解的思路，而不是具体的代码，学会变通。
