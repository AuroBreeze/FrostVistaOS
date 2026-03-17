#include "core/proc.h"
#include "asm/cpu.h"
#include "asm/defs.h"
#include "asm/machine.h"
#include "asm/mm.h"
#include "asm/riscv.h"
#include "kernel/defs.h"
#include "kernel/log.h"

struct cpu cpus[16];
struct Process proc[64];

int cpuid() {
  int id = hal_get_cpu_id();
  return id;
}

struct cpu *get_cpu() {
  int id = cpuid();
  return &cpus[id];
}

void procinit(void) {
  struct Process *p;
  for (p = proc; p < &proc[64]; p++) {
    p->state = UNUSED;
  }
}

static pagetable_t create_user_pagetable() {
  pagetable_t user_pagetable = (pagetable_t)kalloc();
  if (user_pagetable == 0) {
    panic("Failed to allocate memory");
  }

  memset(user_pagetable, 0, PGSIZE);

  // mapping kernel pagetable
  for (int i = 256; i < 512; i++) {
    extern pagetable_t kernel_table;
    user_pagetable[i] = kernel_table[i];
  }

  return user_pagetable;
}

/**
 * alloc_process - Allocate a process
 * Context:
 *
 * Return: if process allocated, return a pointer to the process, else return 0
 * */
struct Process *alloc_process(void) {
  struct Process *p;
  for (p = proc; p < &proc[64]; p++) {
    if (p->state == UNUSED) {
      p->state = USED;
      p->kstack = (uint64)kalloc();
      p->pagetable = create_user_pagetable();

      // NOTE:
      // Position the trapframe above the stack, that is, at a lower address
      // in order to store data in the tramframe
      p->trapframe = (struct trapframe *)(p->kstack - sizeof(struct trapframe));

      extern void usertrapret(void);
      p->context = (struct context *)kalloc();
      p->context->ra = (uint64)usertrapret;

      // NOTE:
      // Point sp to a location not used by the trapframe
      p->context->sp = p->kstack + PGSIZE - sizeof(struct trapframe);
      return p;
    }
  }
  return 0;
}

void user_init() {
  LOG_TRACE("Initializing user process");
  struct Process *p = alloc_process();
  if (p == 0) {
    panic("Failed to allocate process");
  }

  uint64 user_code_table = (uint64)kalloc();
  if (user_code_table == 0) {
    panic("Failed to allocate memory");
  }
  uint64 user_stack = (uint64)kalloc();
  if (user_stack == 0) {
    panic("Failed to allocate memory");
  }
  uint32 user_code[4] = {
      0x00100893, // addi a7, zero, 1
      0x06300513, // addi a0, zero, 99
      0x00000073, // ecall
      0x0000006f  // j .
  };
  memcpy((uint64 *)user_code_table, user_code, 16);

  kvmmap(p->pagetable, 0x0, (uint64)ADR2LOW(user_code_table), PGSIZE,
         PTE_U | PTE_R | PTE_W | PTE_X | PTE_V);
  uint64 user_stack_va = 0x40000;
  kvmmap(p->pagetable, (uint64)user_stack_va, (uint64)ADR2LOW(user_stack),
         PGSIZE, PTE_U | PTE_R | PTE_W | PTE_V);

  uint64 user_stack_top = (uint64)user_stack_va + PGSIZE;
  p->trapframe->sp = user_stack_top;
  p->trapframe->epc = 0x0;

  // Test whether it can be stored in order normally
  p->trapframe->a2 = 666;

  p->state = RUNNABLE;
  LOG_TRACE("User process initialized");
}

struct context scheduler_context;
struct Process *current_proc = 0;

void scheduler(void) {
  struct Process *p;
  extern void swtch(struct context * old, struct context * new);

  for (;;) {
    for (p = proc; p < &proc[64]; p++) {
      if (p->state == RUNNABLE) {
        p->state = RUNNING;
        current_proc = p;

        extern struct trapframe *mytrapframe;
        mytrapframe = p->trapframe;

        // NOTE:
        // Because in uservec, addi sp, sp, -256 is first used, uservec can
        // properly align with the trapframe and store data into it.
        w_sscratch(p->kstack + PGSIZE);

        w_satp(MAKE_SATP(ADR2LOW((uint64)p->pagetable)));
        sfence_vma();

        swtch(&scheduler_context, p->context);

        current_proc = 0;
        extern pagetable_t kernel_table;
        w_satp(MAKE_SATP(kernel_table));
        sfence_vma();

        LOG_TRACE("Switched back to kernel");
      }
    }
  }
  LOG_TRACE("Scheduler done");
}

void yield(void) {
  struct Process *p = current_proc;
  extern void swtch(struct context * old, struct context * new);

  if (current_proc != 0 && current_proc->state == RUNNING) {
    current_proc->state = RUNNABLE;
    swtch(p->context, &scheduler_context);
  }
}
