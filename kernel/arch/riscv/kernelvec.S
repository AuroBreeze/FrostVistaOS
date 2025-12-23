  .section .text
  .global kernelvec
  .align 2
kernelvec:

  addi sp, sp, -256


  # save the registers.
  sd ra, 0(sp)
  sd sp, 8(sp)
  sd gp, 16(sp)
  sd tp, 24(sp)
  sd t0, 32(sp)
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

  call s_trap_handler

  # restore registers.
  ld ra, 0(sp)
  ld sp, 8(sp)
  ld gp, 16(sp)
  ld tp, 24(sp)
  # not tp (contains hartid), in case we moved CPUs
  ld t0, 32(sp)
  ld t1, 40(sp)
  ld t2, 48(sp)
  ld s0, 56(sp)
  ld s1, 64(sp)
  ld a0, 72(sp)
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

  addi sp, sp, 256

  # return to whatever we were doing in the kernel.
  sret


  .global timervec
  .align 4
timervec:
  # Protect kernel timer interrupt context by swapping mscratch and a0
  csrrw a0, mscratch, a0

  # save the register to the memory pointed to by mscratch
  sd a1, 0(a0)
  sd a2, 8(a0)
  sd a3, 16(a0)

  # update mtimecmp (reset alarm)
  # obtain address for CLINT_MTIMECMP and CLINT_MTIME
  li a1, 0x2000000
  li a2, 0xBFF8
  add a2, a1, a2 # a2 = mtime address
  ld a3, 0(a2) # a3 = current time

  li a2, 1000000 # interval
  add a3, a3, a2 # next timer interrupt

  li a2, 0x4000
  add a2, a1, a2 # a2 = mtimecmp address
  sd a3, 0(a2) # write mtimecmp

  # Inject S-state software interrupt (FOrwarding)
  csrr a1, sip
  ori a1, a1, 2 # 1 << 1 (SSIP)
  csrw sip, a1

  #restore the scene
  ld a3, 16(a0)
  ld a2, 8(a0)
  ld a1, 0(a0)
  csrrw a0, mscratch, a0

  mret
