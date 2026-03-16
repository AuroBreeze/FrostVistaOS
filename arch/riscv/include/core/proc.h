#ifndef PROC_H
#define PROC_H

#include "kernel/types.h"

struct trapframe {
  uint64 ra;  // 0(sp)
  uint64 sp;  // 8(sp)
  uint64 gp;  // 16(sp)
  uint64 tp;  // 24(sp)
  uint64 t0;  // 32(sp)
  uint64 t1;  // 40(sp)
  uint64 t2;  // 48(sp)
  uint64 s0;  // 56(sp)
  uint64 s1;  // 64(sp)
  uint64 a0;  // 72(sp)
  uint64 a1;  // 80(sp)
  uint64 a2;  // 88(sp)
  uint64 a3;  // 96(sp)
  uint64 a4;  // 104(sp)
  uint64 a5;  // 112(sp)
  uint64 a6;  // 120(sp)
  uint64 a7;  // 128(sp)
  uint64 s2;  // 136(sp)
  uint64 s3;  // 144(sp)
  uint64 s4;  // 152(sp)
  uint64 s5;  // 160(sp)
  uint64 s6;  // 168(sp)
  uint64 s7;  // 176(sp)
  uint64 s8;  // 184(sp)
  uint64 s9;  // 192(sp)
  uint64 s10; // 200(sp)
  uint64 s11; // 208(sp)
  uint64 t3;  // 216(sp)
  uint64 t4;  // 224(sp)
  uint64 t5;  // 232(sp)
  uint64 t6;  // 240(sp)
};

#endif
