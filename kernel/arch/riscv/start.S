    .section .text.entry
    .globl _start

_start:
    # 1. 设置栈
    la sp, _stack_top

    # 2. 清 .bss 段
    la t0, _bss_start
    la t1, _bss_end
1:
    bgeu t0, t1, 2f
    sd zero, 0(t0)
    addi t0, t0, 8
    j 1b
2:
    # 3. 调用 C 的 main()
    call main

3:
    j 3b    # main 不该返回，防御一下



