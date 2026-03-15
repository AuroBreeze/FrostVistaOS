    .section .text.entry
    .globl _start

_start:
    # 1. set stack pointer
    la sp, _stack_top

    # 2. clear .bss
    la t0, _bss_start
    la t1, _bss_end
1:
    bgeu t0, t1, 2f
    sd zero, 0(t0)
    addi t0, t0, 8
    j 1b
2:
    # 3. call main fucntion
    call mstart

3:
    j 3b    # main does not return, so loop here



