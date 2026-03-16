  .section .text
  .global uservec
  .align 2
uservec:
  # 1. exchange stack. After this, sp=kernel stack, sscratch=user stack
  csrrw sp, sscratch, sp

  # 2. allocate trapframe
  addi sp, sp, -256

  # 3. save t0, because we need to use it to save user sp
  sd t0, 32(sp)

  # 4. save user sp to trapframe
  csrr t0, sscratch
  sd t0, 8(sp)

  # 5. save the registers.
  sd ra, 0(sp)
  sd gp, 16(sp)
  sd tp, 24(sp)
  sd t1, 40(sp)
  sd t2, 48(sp)
  sd s0, 56(sp)
  sd s1, 64(sp)
  sd a0, 72(sp)
  sd a1, 80(sp)
  sd a2, 88(sp)
  sd a3, 96(sp)
  sd a4, 104(sp)
  sd a5, 112(sp)
  sd a6, 120(sp)
  sd a7, 128(sp)
  sd s2, 136(sp)
  sd s3, 144(sp)
  sd s4, 152(sp)
  sd s5, 160(sp)
  sd s6, 168(sp)
  sd s7, 176(sp)
  sd s8, 184(sp)
  sd s9, 192(sp)
  sd s10, 200(sp)
  sd s11, 208(sp)
  sd t3, 216(sp)
  sd t4, 224(sp)
  sd t5, 232(sp)
  sd t6, 240(sp)

  call usertrap

# ==========================================

.global userret
userret:
  mv sp, a0

  addi t0, sp, 256
  csrw sscratch, t0

  # start restore except a0 and sp
  ld ra, 0(sp)
  ld gp, 16(sp)
  ld tp, 24(sp)
  ld t0, 32(sp)
  ld t1, 40(sp)
  ld t2, 48(sp)
  ld s0, 56(sp)
  ld s1, 64(sp)
  ld a1, 80(sp)
  ld a2, 88(sp)
  ld a3, 96(sp)
  ld a4, 104(sp)
  ld a5, 112(sp)
  ld a6, 120(sp)
  ld a7, 128(sp)
  ld s2, 136(sp)
  ld s3, 144(sp)
  ld s4, 152(sp)
  ld s5, 160(sp)
  ld s6, 168(sp)
  ld s7, 176(sp)
  ld s8, 184(sp)
  ld s9, 192(sp)
  ld s10, 200(sp)
  ld s11, 208(sp)
  ld t3, 216(sp)
  ld t4, 224(sp)
  ld t5, 232(sp)
  ld t6, 240(sp)

  # first restore a0, then restore sp that points to trapframe
  ld a0, 72(sp)
  
  # finally restore sp
  ld sp, 8(sp)

  sret
