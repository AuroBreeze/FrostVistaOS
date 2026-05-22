  .global m_trap_handler
  .align 4
m_trap_handler:
   # Protect kernel timer interrupt context by swapping mscratch and a0
  csrrw a0, mscratch, a0

  # save the register to the memory pointed to by mscratch
  sd ra, 0(a0)
  sd sp, 8(a0)
  sd gp, 16(a0)
  sd tp, 24(a0)
  sd t0, 32(a0)

  csrr t0, mscratch # save mscratch in t0
  sd t0, 72(a0)

  sd t1, 40(a0)
  sd t2, 48(a0)
  sd s0, 56(a0)
  sd s1, 64(a0)
  sd a1, 80(a0)
  sd a2, 88(a0)
  sd a3, 96(a0)
  sd a4, 104(a0)
  sd a5, 112(a0)
  sd a6, 120(a0)
  sd a7, 128(a0)
  sd s2, 136(a0)
  sd s3, 144(a0)
  sd s4, 152(a0)
  sd s5, 160(a0)
  sd s6, 168(a0)
  sd s7, 176(a0)
  sd s8, 184(a0)
  sd s9, 192(a0)
  sd s10, 200(a0)
  sd s11, 208(a0)
  sd t3, 216(a0)
  sd t4, 224(a0)
  sd t5, 232(a0)
  sd t6, 240(a0)

  mv s0, a0

  csrr a0, mcause
  csrr a1, mepc
  mv a2, s0

  csrr t0, mhartid         # Get current CPU ID
  slli t0, t0, 12          # Multiply hartid by 4096 (left shift by 12)
  la sp, m_trap_stack_top
  sub sp, sp, t0           # Adjust stack pointer for the specific CPU

  call m_trap

  mv a0, s0

   # restore the state
  # restore registers.
  ld ra, 0(a0)
  ld sp, 8(a0)
  ld gp, 16(a0)
  ld tp, 24(a0)
  ld t0, 32(a0)
  ld t1, 40(a0)
  ld t2, 48(a0)
  ld s0, 56(a0)
  ld s1, 64(a0)
  ld a1, 80(a0)
  ld a2, 88(a0)
  ld a3, 96(a0)
  ld a4, 104(a0)
  ld a5, 112(a0)
  ld a6, 120(a0)
  ld a7, 128(a0)
  ld s2, 136(a0)
  ld s3, 144(a0)
  ld s4, 152(a0)
  ld s5, 160(a0)
  ld s6, 168(a0)
  ld s7, 176(a0)
  ld s8, 184(a0)
  ld s9, 192(a0)
  ld s10, 200(a0)
  ld s11, 208(a0)
  ld t3, 216(a0)
  ld t4, 224(a0)
  ld t5, 232(a0)
  ld t6, 240(a0)

  csrrw a0, mscratch, a0

  mret
