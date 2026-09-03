  .section .text
  .global userret
  .align 2

  .type userret, @function

.equ TRAPFRAME_SP, 16

# void userret(struct arch_trapframe *tf)
# Context: return from a LoongArch user trap.
userret:
  move    $sp, $a0

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
  ld.d    $sp, $sp, TRAPFRAME_SP

  ertn
