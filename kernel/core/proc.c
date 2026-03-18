#include "core/proc.h"
#include "asm/cpu.h"
#include "asm/defs.h"
#include "asm/machine.h"
#include "asm/mm.h"
#include "asm/riscv.h"
#include "kernel/defs.h"
#include "kernel/kalloc.h"
#include "kernel/log.h"

struct cpu cpus[16];
struct Process proc[64];
int pid = 0;

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

int get_pid() { return pid++; }

static pagetable_t create_user_pagetable() {
  pagetable_t user_pagetable = (pagetable_t)kalloc();
  if (user_pagetable == 0) {
    panic("Failed to allocate memory");
  }

  // mapping kernel pagetable
  for (int i = 1; i < 512; i++) {
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
      p->pid = get_pid();
      // TODO: Implement stack protection by allocating an extra page
      p->kstack = (uint64)kalloc();
      p->pagetable = create_user_pagetable();

      if (p->kstack == 0 || p->pagetable == 0) {
        panic("Alloc process: Failed to allocate memory");
      }

      // NOTE:
      // Position the trapframe above the stack, that is, at a lower address
      // in order to store data in the tramframe
      p->trapframe =
          (struct trapframe *)(p->kstack + PGSIZE - sizeof(struct trapframe));

      extern void usertrapret(void);
      // NOTE: p->context must be allocated in the kernel otherwise it will be
      // panic
      p->context = (struct context *)kalloc();
      if (p->context == 0) {
        panic("Alloc process: Failed to allocate memory");
      }
      p->context->ra = (uint64)usertrapret;

      // NOTE:
      // Point sp to a location not used by the trapframe
      p->context->sp = (uint64)(p->trapframe) - sizeof(struct context);
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

  // uint32 user_code[3] = {
  //     0x00200893, // li a7, 2
  //     0x00000073, // ecall
  //
  //     // 0x00050663, // beqz a0, 12
  //     // 0xAAA00593, // li a1, 0xAAA
  //     0x0000006f // j .
  //                // 0xBBB00593, // li a1, 0xBBB
  //                // 0x0000006f, // j .
  // };

  uint32 user_code[] = {
      // 1. 调用 fork (syscall 2)
      0x00200893, // [0x00] li a7, 2
      0x00000073, // [0x04] ecall         (返回值 a0 = PID 或 0)

      // 2. 根据 a0 的值分流
      0x00050a63, // [0x08] beqz a0, 20   (如果是子进程 a0==0，跳过 20 字节到
                  // 0x1C)

      // ==========================================
      // 3. 父进程死循环区 (不断打印 222)
      // ==========================================
      0x0de00513, // [0x0C] li a0, 222    (设置参数 a0 = 222)
      0x00100893, // [0x10] li a7, 1      (设置系统调用号 a7 = 1)
      0x00000073, // [0x14] ecall         (执行 syscall 1)
      0xff5ff06f, // [0x18] j -12         (往回跳 12 个字节，跳回 0x0C)

      // ==========================================
      // 4. 子进程死循环区 (不断打印 111)
      // ==========================================
      0x06f00513, // [0x1C] li a0, 111    (设置参数 a0 = 111)
      0x00100893, // [0x20] li a7, 1      (设置系统调用号 a7 = 1)
      0x00000073, // [0x24] ecall         (执行 syscall 1)
      0xff5ff06f  // [0x28] j -12         (往回跳 12 个字节，跳回 0x1C)
  };
  memcpy((uint64 *)user_code_table, user_code, sizeof(user_code));

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
        LOG_TRACE("Switching to process %d", p->pid);

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

        // NOTE:
        // Ensure that the value written to the register is the actual physical
        // address
        w_satp(MAKE_SATP(ADR2LOW(kernel_table)));
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

void freeproc(struct Process *p) {
  p->state = UNUSED;
  p->pid = 0;
  p->name[0] = 0;

  kfree((void *)p->kstack);
  p->kstack = 0;

  // TODO: Completely clear the space in the page table and release it
  p->pagetable = 0;

  kfree((void *)p->context);
  p->context = 0;
  p->trapframe = 0;
  p->size = 0;
}

int fork() {
  LOG_TRACE("Forking");
  struct Process *np = alloc_process();
  struct Process *p = current_proc;
  if (np == 0) {
    return -1;
  }

  if (!uvmcopy(p->pagetable, np->pagetable)) {
    freeproc(np);
    return -1;
  }

  np->size = p->size;

  *(np->trapframe) = *(p->trapframe);
  np->trapframe->a0 = 0;

  // Prevent it from getting stuck there and failing to proceed
  np->trapframe->epc += 4;

  np->state = RUNNABLE;
  LOG_TRACE("Forked process %d", np->pid);

  return np->pid;
}
