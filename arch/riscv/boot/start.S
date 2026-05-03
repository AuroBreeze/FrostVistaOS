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

    .section .bss.m_trap_stack
    .align 12
    .global m_trap_stack
m_trap_stack:
    .space 0x4000  # 16KB
    
    .global m_trap_stack_top
m_trap_stack_top:
