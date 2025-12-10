/* start.S */
    .section .text
    .global _start
_start:
    /* set stack pointer */
    la sp, _stack_top

    /* clear .bss segmation*/
    la t0, _bss_start
    la t1, _bss_end
1:  bgeu t0, t1, 2f     /* if(t0>=t1) break;*/
    sd zero, 0(t0)      /* RV64: 8byte clear*/
    addi t0, t0, 8    /* t0 += 8 */
    j 1b
2: 
    /* call C's main() */
    call main
/* if main return that set dead circle*/
3:
    j 3b
