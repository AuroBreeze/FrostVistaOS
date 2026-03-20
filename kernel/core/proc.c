#include "core/proc.h"
#include "asm/cpu.h"
#include "asm/defs.h"
#include "asm/mm.h"
#include "asm/riscv.h"
#include "asm/trap.h"
#include "kernel/defs.h"
#include "kernel/log.h"

struct cpu cpus[16];
struct Process proc[64];
struct context scheduler_context;
struct Process *current_proc = 0;
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

// void user_init() {
//   LOG_TRACE("Initializing user process");
//   struct Process *p = alloc_process();
//   if (p == 0) {
//     panic("Failed to allocate process");
//   }
//
//   uint64 user_code_table = (uint64)kalloc();
//   if (user_code_table == 0) {
//     panic("Failed to allocate memory");
//   }
//   uint64 user_stack = (uint64)kalloc();
//   if (user_stack == 0) {
//     panic("Failed to allocate memory");
//   }
//
//   uint32 user_code[] = {
//       // 1. Ecall fork (syscall 2)
//       0x00200893, // [0x00] li a7, 2
//       0x00000073, // [0x04] ecall
//
//       // 2. Traffic diversion (Branch jump)
//       // [Modification 1]: Parent process grew by 8 bytes, so the jump offset
//       is
//       // now 28 bytes (0x1C)
//       // Machine code for 'beqz a0, 28' is 0x00050e63
//       0x00050e63, // [0x08] beqz a0, 28
//
//       // ================= Parent Process =================
//       // 3. Ecall wait (syscall 4)
//       // [Modification 2]: Parent calls wait() first to reap the zombie
//       child! 0x00400893, // [0x0C] li a7, 4 (sys_wait) 0x00000073, // [0x10]
//       ecall
//
//       // 4. After reaping the child, start an infinite loop printing 222
//       0x0de00513, // [0x14] li a0, 222
//       0x00100893, // [0x18] li a7, 1
//       0x00000073, // [0x1C] ecall
//       0xff5ff06f, // [0x20] j -12  (Jumps exactly back to 'li a0, 222' at
//       0x14)
//
//       // ================= Child Process =================
//       // 5. Print 111 once
//       0x06f00513, // [0x24] li a0, 111
//       0x00100893, // [0x28] li a7, 1
//       0x00000073, // [0x2C] ecall
//
//       // 6. Ecall exit (syscall 3)
//       0x00300893, // [0x30] li a7, 3  (sys_exit)
//       0x00000073, // [0x34] ecall
//   };
//   memcpy((uint64 *)user_code_table, user_code, sizeof(user_code));
//
//   kvmmap(p->pagetable, 0x0, (uint64)VA2PA(user_code_table), PGSIZE,
//          PTE_U | PTE_R | PTE_W | PTE_X | PTE_V);
//   uint64 user_stack_va = 0x40000;
//   kvmmap(p->pagetable, (uint64)user_stack_va, (uint64)VA2PA(user_stack),
//   PGSIZE,
//          PTE_U | PTE_R | PTE_W | PTE_V);
//
//   uint64 user_stack_top = (uint64)user_stack_va + PGSIZE;
//   p->size = user_stack_top + 0x1000;
//   p->trapframe->sp = user_stack_top;
//   p->trapframe->epc = 0x0;
//
//   // Test whether it can be stored in order normally
//   p->trapframe->a2 = 666;
//
//   p->state = RUNNABLE;
//   LOG_TRACE("User process initialized");
// }

void user_init() {
  struct Process *p = alloc_process();
  if (p == 0) {
    panic("Failed to allocate process");
  }

  current_proc = p;
  if (exec() == 0) {
    panic("Failed to exec");
  }

  p->state = RUNNABLE;
  current_proc = 0;

  LOG_TRACE("User process initialized");
}
void scheduler(void) {
  struct Process *p;
  extern void swtch(struct context * old, struct context * new);

  for (;;) {
    intr_on();
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

        w_satp(MAKE_SATP(VA2PA((uint64)p->pagetable)));
        sfence_vma();

        swtch(&scheduler_context, p->context);

        current_proc = 0;
        extern pagetable_t kernel_table;

        // NOTE:
        // Ensure that the value written to the register is the actual physical
        // address
        w_satp(MAKE_SATP(VA2PA(kernel_table)));
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

  if (p->kstack) {
    kfree((void *)p->kstack);
    p->kstack = 0;
  }

  if (p->pagetable) {
    uvmfree(p->pagetable, p->size);
    p->pagetable = 0;
  }

  if (p->context) {
    kfree((void *)p->context);
    p->context = 0;
  }
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

  // Why this can completely copy the trapframe?
  *(np->trapframe) = *(p->trapframe);
  np->trapframe->a0 = 0;
  np->parent = p;
  // Prevent it from getting stuck there and failing to proceed
  np->trapframe->epc += 4;

  np->state = RUNNABLE;
  LOG_TRACE("Forked process %d", np->pid);

  return np->pid;
}

int exit() {
  struct Process *current;
  struct Process *p;

  current = current_proc;
  for (int i = 0; i < 64; i++) {
    p = &proc[i];
    if (p->parent == current) {
      p->parent = &proc[0];
    }
  }

  current->state = ZOMBIE;
  LOG_TRACE("Process %d exited", current->pid);

  // FIXME: Since the current conditions are not met, we must force a switch to
  // another process.
  extern void swtch(struct context * old, struct context * new);
  swtch(current->context, &scheduler_context);

  panic("zombie exit: return from swtch");

  return 0;
}

/**
 * wait - wait for a child process to exit
 *
 * Context: Only wait one child
 */
int wait() {
  struct Process *cur;
  struct Process *p;
  int havekids;
  int child_pid;

  child_pid = -1;
  cur = current_proc;

  for (;;) {
    // The “havekids” field must be included
    havekids = 0;
    for (int i = 0; i < 64; i++) {
      p = &proc[i];
      if (p->parent == cur) {
        havekids++;
        if (p->state == ZOMBIE) {
          child_pid = p->pid;
          freeproc(p);
          return child_pid;
        }
      }
    }
    if (havekids == 0) {
      return -1;
    }
    yield();
  }
}
