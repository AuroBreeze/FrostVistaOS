  .section .text
  .global kernelvec
  .align 12

  .type kernelvec, @function

.equ TRAPFRAME_SIZE, 32*8
.equ PRMD, 0x1
.equ PPLV_MASK, 0x3
.equ SAVE0, 0x30 # 规定save0中保存内核 sp 指针
.equ SAVE1, 0x31
.equ SAVE2, 0x32
.equ SAVE3, 0x33

kernelvec:
  csrwr  $t0, SAVE1         # 优先保存原始 t0
  csrwr  $sp, SAVE2         # 保存原始 sp
  csrwr  $t1, SAVE3         # 保存原始 t1，下面需要使用 t1 比较 PPLV

  csrrd  $t0, PRMD          # PRMD
  andi   $t0, $t0, PPLV_MASK # PPLV

  beqz   $t0, from_kernel
  # 当前只接受 PLV3
  addi.w $t1, $zero, 3
  bne    $t0, $t1, invalid_plv
  b      from_user

invalid_plv:
  idle   0
  b      invalid_plv

from_user:
  csrrd  $sp, SAVE0         # 读取当前进程的内核栈顶
  addi.d  $sp, $sp, -TRAPFRAME_SIZE

  # 保存原始用户 sp
  csrrd   $t0, SAVE2
  st.d    $t0, $sp, 16

  # 保存原始 t0
  csrrd   $t0, SAVE1
  st.d    $t0, $sp, 88

  b       save_remaining_registers

from_kernel:
  csrrd   $sp, SAVE2
  # 保持当前 kernel sp
  addi.d  $sp, $sp, -TRAPFRAME_SIZE

  # 保存原始 kernel sp
  csrrd   $t0, SAVE2
  st.d    $t0, $sp, 16

  # 保存原始 t0
  csrrd   $t0, SAVE1
  st.d    $t0, $sp, 88

  b       save_remaining_registers

save_remaining_registers:

  st.d    $ra, $sp, 0
  st.d    $tp, $sp, 8

  csrrd   $t0, SAVE3
  st.d    $t0, $sp, 96
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

  bl      arch_trap_handle

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
