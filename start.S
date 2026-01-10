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