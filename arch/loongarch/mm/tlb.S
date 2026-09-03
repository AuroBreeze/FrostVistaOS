  .section .text.boot
  .global tlb_entry
  .align 12

  .type tlb_entry, @function

.equ TRAPFRAME_SIZE, 32*8
.equ TLBRSAVE, 0x8B

tlb_entry:

  # TLBRSAVE 先保存原始 t0；异常前的 sp 可能仍指向高半区栈。
  csrwr   $t0, TLBRSAVE
  or      $t0, $sp, $zero
  /*
   * 切换到 DMW0 下的启动栈。
   *
   * _boot_stack_top 位于：
   *   0x8000000000000000 + physical address
   */
  la.global   $sp, _boot_stack_top
  addi.d  $sp, $sp, -TRAPFRAME_SIZE

  st.d    $ra, $sp, 0
  st.d    $tp, $sp, 8

  # t0 此时仍保存异常发生前的 sp，必须在读取 TLBRSAVE 前先保存。
  st.d    $t0, $sp, 16

  csrrd   $t0, TLBRSAVE
  st.d    $t0, $sp, 88
  st.d    $t1, $sp, 96
  st.d    $t2, $sp, 104
  st.d    $t3, $sp, 112
  st.d    $t4, $sp, 120
  st.d    $t5, $sp, 128
  st.d    $t6, $sp, 136
  st.d    $t7, $sp, 144
  st.d    $t8, $sp, 152

  st.d    $a0, $sp, 24
  st.d    $a1, $sp, 32
  st.d    $a2, $sp, 40
  st.d    $a3, $sp, 48
  st.d    $a4, $sp, 56
  st.d    $a5, $sp, 64
  st.d    $a6, $sp, 72
  st.d    $a7, $sp, 80

  st.d    $r21, $sp, 160
  st.d    $fp, $sp, 168
  st.d    $s0, $sp, 176
  st.d    $s1, $sp, 184
  st.d    $s2, $sp, 192
  st.d    $s3, $sp, 200
  st.d    $s4, $sp, 208
  st.d    $s5, $sp, 216
  st.d    $s6, $sp, 224
  st.d    $s7, $sp, 232
  st.d    $s8, $sp, 240

  bl      boot_tlb_refill_handler
  beqz    $a0, tlb_refill_failed
  tlbfill

  ld.d    $ra, $sp, 0
  ld.d    $tp, $sp, 8
  ld.d    $a0, $sp, 24
  ld.d    $a1, $sp, 32
  ld.d    $a2, $sp, 40
  ld.d    $a3, $sp, 48
  ld.d    $a4, $sp, 56
  ld.d    $a5, $sp, 64
  ld.d    $a6, $sp, 72
  ld.d    $a7, $sp, 80
  ld.d    $t0, $sp, 88
  ld.d    $t1, $sp, 96
  ld.d    $t2, $sp, 104
  ld.d    $t3, $sp, 112
  ld.d    $t4, $sp, 120
  ld.d    $t5, $sp, 128
  ld.d    $t6, $sp, 136
  ld.d    $t7, $sp, 144
  ld.d    $t8, $sp, 152
  ld.d    $r21, $sp, 160
  ld.d    $fp, $sp, 168
  ld.d    $s0, $sp, 176
  ld.d    $s1, $sp, 184
  ld.d    $s2, $sp, 192
  ld.d    $s3, $sp, 200
  ld.d    $s4, $sp, 208
  ld.d    $s5, $sp, 216
  ld.d    $s6, $sp, 224
  ld.d    $s7, $sp, 232
  ld.d    $s8, $sp, 240
  ld.d    $sp, $sp, 16

  ertn

tlb_refill_failed:
  # C 处理器对未映射地址负责报告错误；正常路径不会执行到这里。
  b       tlb_refill_failed
