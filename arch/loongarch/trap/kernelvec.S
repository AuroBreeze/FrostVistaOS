  .section .text
  .global kernelvec
  .align 12

  .type kernelvec, @function

.equ Save0, 0x30

kernelvec:
  addi.d  $sp, $sp, -16

  st.d    $ra, $sp, 0
  st.d    $a0, $sp, 8

  bl      trap_handler

  ld.d    $ra, $sp, 0
  ld.d    $a0, $sp, 8

  addi.d  $sp, $sp, 16

  ertn
