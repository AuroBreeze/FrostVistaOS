#ifndef RISCV_H
#define RISCV_H

#include "kernel/types.h"

#define MSTATUS_MPP_MASK                                                       \
  (3ULL << 11) // achieving the effect of masking through inversion
#define MSTATUS_MPP_S                                                          \
  (1ULL << 11) // set MPP bit to 1 switch the privilege level

#ifndef MSTATUS_MIE

#define MSTATUS_MIE (1ULL << 3)

#endif

// set pmpcfg and pmpaddr to configer the addresses accessible in S mode
// For details, refer to the Pyysical Address Protection section of the RISCV
// Privilege Manual.
static inline void w_pmpaddr0(uint64 x) {
  asm volatile("csrw pmpaddr0, %0" ::"r"(x));
}

static inline void w_pmpcfg0(uint64 x) {
  asm volatile("csrw pmpcfg0, %0" ::"r"(x));
}

static inline uint64 r_mstatus(void) {
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r"(x));
  return x;
}

static inline void w_mstatus(uint64 x) {
  asm volatile("csrw mstatus, %0" ::"r"(x));
}

static inline void w_mepc(uint64 x) {
  asm volatile("csrw mepc, %0" : : "r"(x));
}

static inline void w_medeleg(uint64 x) {
  asm volatile("csrw medeleg, %0" : : "r"(x));
}
static inline void w_mideleg(uint64 x) {
  asm volatile("csrw mideleg, %0" : : "r"(x));
}

// set the mode bit in satp to configure different paging modes.
static inline void w_satp(uint64 x) {
  asm volatile("csrw satp, %0" : : "r"(x));
}
static inline uint64 r_satp(void) {
  uint64 x;
  asm volatile("csrr %0, satp" : "=r"(x));
  return x;
}
// Force TLB Refresh
static inline void sfence_vma(void) { asm volatile("sfence.vma zero, zero"); }

static inline uint64 r_sstatus(void) {
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r"(x));
  return x;
}

static inline void w_sstatus(uint64 x) {
  asm volatile("csrw sstatus, %0" : : "r"(x));
}

static inline void w_stvec(uint64 x) {
  asm volatile("csrw stvec, %0" : : "r"(x));
}

static inline uint64 r_scause(void) {
  uint64 x;
  asm volatile("csrr %0, scause" : "=r"(x));
  return x;
}

static inline uint64 r_sepc(void) {
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r"(x));
  return x;
}
static inline void w_sepc(uint64 x) {
  asm volatile("csrw sepc, %0" : : "r"(x));
}

static inline uint64 r_stval(void) {
  uint64 x;
  asm volatile("csrr %0, stval" : "=r"(x));
  return x;
}
static inline void w_mtvec(uint64 x) {
  asm volatile("csrw mtvec, %0" : : "r"(x));
}
static inline uint64 r_mtvec(void) {
  uint64 x;
  asm volatile("csrr %0, mtvec" : "=r"(x));
  return x;
}
static inline uint64 r_mhartid(void) {
  uint64 x;
  asm volatile("csrr %0, mhartid" : "=r"(x));
  return x;
}

static inline void w_mhartid(uint64 x) {
  asm volatile("csrw hartid, %0" : : "r"(x));
}

static inline void w_mscratch(uint64 x) {
  asm volatile("csrw mscratch, %0" : : "r"(x));
}

static inline void w_mie(uint64 x) { asm volatile("csrw mie, %0" : : "r"(x)); }

static inline uint64 r_mie(void) {
  uint64 x;
  asm volatile("csrr %0, mie" : "=r"(x));
  return x;
}

static inline void w_sie(uint64 x) { asm volatile("csrw sie, %0" : : "r"(x)); }

static inline uint64 r_sie(void) {
  uint64 x;
  asm volatile("csrr %0, sie" : "=r"(x));
  return x;
}

static inline void w_sip(uint64 x) { asm volatile("csrw sip, %0" : : "r"(x)); }

static inline uint64 r_sip(void) {
  uint64 x;
  asm volatile("csrr %0, sip" : "=r"(x));
  return x;
}

// WARNING: Leap of Faith
static inline void switch_to_high_address(uint64 high_target,
                                          uint64 virt_offset) {
  asm volatile("sfence.vma\n\t"
               // WARNING: Raise the SP pointer to a high address
               "add sp, sp, %1\n\t"
               "jr %0\n\t"
               :
               : "r"(high_target), "r"(virt_offset)
               : "memory");
}

#endif
