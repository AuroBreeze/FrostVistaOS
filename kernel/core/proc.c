#include "core/proc.h"
#include "asm/cpu.h"
#include "asm/defs.h"
#include "asm/mm.h"
#include "asm/riscv.h"
#include "asm/trap.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/spinlock.h"

struct cpu cpus[16];
struct Process proc[64];
struct context scheduler_context;

struct spinlock pid_lock = {.name = "pid_lock", .locked = 0, .cpu = 0};
struct spinlock proc_lock = {.name = "proc_lock", .locked = 0, .cpu = 0};

int pid = 0;

int cpuid() {
  int id = hal_get_cpu_id();
  return id;
}

struct cpu *get_cpu() {
  int id = cpuid();
  return &cpus[id];
}

struct Process *get_proc() {
  struct cpu *c = get_cpu();
  struct Process *p = c->proc;
  return p;
}

void procinit(void) {
  struct Process *p;
  for (p = proc; p < &proc[64]; p++) {
    p->state = UNUSED;
  }
}

int get_pid() {
  int p;
  acquire(&pid_lock);
  p = pid++;
  release(&pid_lock);
  return p;
}

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
  acquire(&proc_lock);
  for (p = proc; p < &proc[64]; p++) {
    if (p->state == UNUSED) {
      p->state = USED;
      p->pid = get_pid();
      release(&proc_lock);
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
  release(&proc_lock);
  return 0;
}

void user_init() {
  struct Process *p = alloc_process();
  if (p == 0) {
    panic("Failed to allocate process");
  }

  struct cpu *c = get_cpu();
  c->proc = p;

  if (p == 0) {
    panic("Failed to allocate process");
  }

  if (exec() == 0) {
    panic("Failed to exec");
  }

  p->state = RUNNABLE;
  c->proc = 0;

  LOG_TRACE("User process initialized");
}

void scheduler(void) {
  struct Process *p;
  extern void swtch(struct context * old, struct context * new);

  for (;;) {
    intr_on();
    for (p = proc; p < &proc[64]; p++) {
      acquire(&proc_lock);
      if (p->state == RUNNABLE) {
        p->state = RUNNING;
        LOG_TRACE("Switching to process %d", p->pid);

        struct Process *myproc = p;
        struct cpu *c = get_cpu();
        c->proc = myproc;

        struct trapframe *trapframe = myproc->trapframe;

        trapframe = p->trapframe;

        // NOTE:
        // Because in uservec, addi sp, sp, -256 is first used, uservec can
        // properly align with the trapframe and store data into it.
        w_sscratch(p->kstack + PGSIZE);

        w_satp(MAKE_SATP(VA2PA((uint64)p->pagetable)));
        sfence_vma();

        swtch(&scheduler_context, p->context);

        c->proc = 0;

        extern pagetable_t kernel_table;

        // NOTE:
        // Ensure that the value written to the register is the actual physical
        // address
        w_satp(MAKE_SATP(VA2PA(kernel_table)));
        sfence_vma();

        LOG_TRACE("Switched back to kernel");
      }
      release(&proc_lock);
    }
  }
  LOG_TRACE("Scheduler done");
}

void yield(void) {
  struct Process *current_proc = get_proc();
  extern void swtch(struct context * old, struct context * new);

  if (current_proc != 0 && current_proc->state == RUNNING) {
    acquire(&proc_lock);
    current_proc->state = RUNNABLE;
    swtch(current_proc->context, &scheduler_context);
    release(&proc_lock);
  }
}

void freeproc(struct Process *p) {
  acquire(&proc_lock);
  p->pid = 0;
  p->name[0] = 0;

  if (p->kstack) {
    kfree((void *)p->kstack);
    p->kstack = 0;
  }

  if (p->pagetable) {
    uvmfree(p->pagetable, p);
    p->pagetable = 0;
  }

  if (p->context) {
    kfree((void *)p->context);
    p->context = 0;
  }
  p->trapframe = 0;
  p->size = 0;

  p->state = UNUSED;
  release(&proc_lock);
}

int fork() {
  LOG_TRACE("Forking");
  struct Process *np = alloc_process();
  struct Process *p = get_proc();
  if (np == 0) {
    return -1;
  }

  acquire(&proc_lock);
  if (!uvmcopy(p->pagetable, np->pagetable)) {
    freeproc(np);
    return -1;
  }

  np->heap_top = p->heap_top;
  np->heap_bottom = p->heap_bottom;
  np->stack_top = p->stack_top;
  np->stack_bottom = p->stack_bottom;

  // Why this can completely copy the trapframe?
  *(np->trapframe) = *(p->trapframe);
  np->trapframe->a0 = 0;
  np->parent = p;
  // Prevent it from getting stuck there and failing to proceed
  np->trapframe->epc += 4;

  np->state = RUNNABLE;
  release(&proc_lock);
  LOG_TRACE("Forked process %d", np->pid);

  return np->pid;
}

int exit() {
  struct Process *current;
  struct Process *p;

  current = get_proc();
  for (int i = 0; i < 64; i++) {
    p = &proc[i];
    if (p->parent == current) {
      p->parent = &proc[0];
    }
  }

  acquire(&proc_lock);
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
  cur = get_proc();

  for (;;) {
    // The “havekids” field must be included
    havekids = 0;
    acquire(&proc_lock);
    for (int i = 0; i < 64; i++) {
      p = &proc[i];
      if (p->parent == cur) {
        havekids++;
        if (p->state == ZOMBIE) {
          child_pid = p->pid;
          release(&proc_lock);

          freeproc(p);
          return child_pid;
        }
      }
    }
    release(&proc_lock);
    if (havekids == 0) {
      return -1;
    }
    yield();
  }
}

uint64 sbrk(int64 size) {
  struct Process *cur;
  uint64 old_head_top, new_head_top;

  cur = get_proc();
  old_head_top = cur->heap_top;
  new_head_top = old_head_top + size;

  LOG_TRACE("sbrk: old_head_top %p, new_head_top %p, size %d",
            (void *)old_head_top, (void *)new_head_top, size);

  if (size < 0 && new_head_top > old_head_top) {
    return 0;
  }
  if (size == 0) {
    return old_head_top;
  }

  if (size < 0) {
    acquire(&proc_lock);
    if (!uvmdealloc(cur->pagetable, old_head_top, size))
      return 0;
    release(&proc_lock);
  }

  acquire(&proc_lock);
  cur->heap_top = new_head_top;
  release(&proc_lock);
  LOG_TRACE("sbrk: success");
  return old_head_top;
}
