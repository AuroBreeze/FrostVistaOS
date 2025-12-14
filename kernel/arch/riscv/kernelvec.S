  .section .text
  .global kernelvec
  .align 2
kernelvec:
  call s_trap_handler

1:
  j 1b
