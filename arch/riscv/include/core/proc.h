#ifndef PROC_H
#define PROC_H

#include "kernel/types.h"
// kernel
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

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

  uint64 epc;
};

// Per-CPU state.
struct cpu {
  struct Process *proc;   // The process running on this cpu, or null.
  struct context context; // swtch() here to enter scheduler().
  int noff;               // Record nesting depth
  int intena; // Record the interrupt status before the first interrupt is
              // disabled
};

enum proc_state { UNUSED, USED, RUNNABLE, RUNNING, ZOMBIE };

struct Process {
  enum proc_state state;
  int pid;       // Process ID
  char name[16]; // Process name

  uint64 kstack;               // Kernel stack pointer
  struct Process *parent;      // Parent process
  pagetable_t pagetable;       // Page table
  struct context *context;     // Kernel context
  struct trapframe *trapframe; // User trap frame

  uint64 size; // Size of process memory but remove from the stack
  uint64 heap_bottom;
  uint64 heap_top;

  uint64 stack_bottom;
  uint64 stack_top; // Upper boundary in the pagetable, Usually PHYSTOP_LOW
};

extern int pid;

void user_init();
void procinit(void);
void scheduler(void);
void yield(void);
int fork();
int exit();
int wait();
uint64 sbrk(int64);

#endif
