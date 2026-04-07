#include "core/proc.h"
#include "asm/cpu.h"
#include "asm/defs.h"
#include "asm/mm.h"
#include "asm/riscv.h"
#include "asm/trap.h"
#include "kernel/defs.h"
#include "kernel/fcntl.h"
#include "kernel/log.h"
#include "kernel/spinlock.h"
#define NFILE 128

file_t ftable[NFILE];

struct cpu cpus[NCPU];
struct Process proc[NPROC];
extern pagetable_t kernel_table;

struct spinlock pid_lock = {.name = "pid_lock", .locked = 0, .cpu = 0};

int pid = 0;

int file_alloc() {
  for (int i = 0; i < NFILE; i++) {
    if (ftable[i].ref_count == 0) {
      return i;
    }
  }
  return -1;
}

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
  for (p = proc; p < &proc[NPROC]; p++) {
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

/**
 * alloc_process - Allocate a process
 *
 * Context:
 *
 * Return: if process allocated, return a pointer to the process, else return 0
 * */
struct Process *alloc_process(void) {
  struct Process *p;
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == UNUSED) {
      p->state = USED;
      p->pid = get_pid();
      release(&p->lock);
      // TODO: Implement stack protection by allocating an extra page
      p->kstack = (uint64)kalloc();
      p->pagetable = uvmcreate();

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

      for (int i = 0; i < 16; i++) {
        p->ofile[i] = 0;
      }

      // NOTE:
      // Point sp to a location not used by the trapframe
      p->context->sp = (uint64)(p->trapframe) - sizeof(struct context);
      return p;
    }
    release(&p->lock);
  }
  return 0;
}

void user_init() {
  struct Process *p = alloc_process();
  if (p == 0) {
    panic("Failed to allocate process");
  }
  extern vfs_node_t *vfs_root;
  vfs_node_t *tty = vfs_lookup(vfs_root, "/dev/tty");

  int fid = file_alloc();
  if (fid < 0)
    panic("no files");

  file_t *f = &ftable[fid];
  f->node = tty;
  f->writable = 1;
  f->readable = 1;
  f->offset = 0;
  f->ref_count = 3; // stdin(0), stdout(1), stderr(2)

  p->ofile[0] = f;
  p->ofile[1] = f;
  p->ofile[2] = f;

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
    for (p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if (p->state == RUNNABLE) {
        p->state = RUNNING;
        LOG_TRACE("Switching to process %d", p->pid);

        struct Process *myproc = p;
        struct cpu *c = get_cpu();
        c->proc = myproc;

        struct trapframe *trapframe = myproc->trapframe;

        trapframe = p->trapframe;

        // Because in uservec, addi sp, sp, -256 is first used, uservec can
        // properly align with the trapframe and store data into it.
        w_sscratch(p->kstack + PGSIZE);

        w_satp(MAKE_SATP(VA2PA((uint64)p->pagetable)));
        sfence_vma();

        swtch(&c->context, p->context);

        c->proc = 0;

        // Ensure that the value written to the register is the actual physical
        // address
        w_satp(MAKE_SATP(VA2PA(kernel_table)));
        sfence_vma();

        LOG_TRACE("Switched back to kernel");
      }
      // The lock will be reacquired in the `yield` block
      // So what is actually being released here is the lock added by `yield` or
      // `swtch`.
      release(&p->lock);
    }
  }
  LOG_TRACE("Scheduler done");
}

/**
 * sched - Switch to the next process
 *
 * Context: Must be holding the proc_lock before calling
 *
 */
void sched(void) {
  int intena;
  struct Process *p = get_proc();

  if (!holding(&p->lock))
    panic("sched p->lock");
  if (get_cpu()->noff != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (intr_get())
    panic("sched interruptible");

  intena = get_cpu()->intena;

  extern void swtch(struct context * old, struct context * new);

  // Switch back to the CPU's context
  swtch(p->context, &get_cpu()->context);
  get_cpu()->intena = intena;
}

/**
 * yield - Yield the CPU
 *
 * Context: Will switch back to the CPU's context and return to the scheduler
 */
void yield(void) {
  struct Process *current_proc = get_proc();

  if (current_proc != 0 && current_proc->state == RUNNING) {
    acquire(&current_proc->lock);
    current_proc->state = RUNNABLE;
    sched();
    release(&current_proc->lock);
  }
}

int alloc_fd(struct Process *p, file_t *f) {
  for (int i = 0; i < 16; i++) {
    if (p->ofile[i] == 0) {
      p->ofile[i] = f;
      return i;
    }
  }
  return -1;
}

void freeproc(struct Process *p) {
  acquire(&p->lock);
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
  release(&p->lock);
}

int fork() {
  LOG_TRACE("Forking");
  struct Process *np = alloc_process();
  struct Process *p = get_proc();
  if (np == 0) {
    return -1;
  }

  acquire(&np->lock);
  if (!uvmcopy(p->pagetable, np->pagetable)) {
    freeproc(np);
    return -1;
  }

  np->heap_top = p->heap_top;
  np->heap_bottom = p->heap_bottom;
  np->stack_top = p->stack_top;
  np->stack_bottom = p->stack_bottom;

  *(np->trapframe) = *(p->trapframe);
  np->trapframe->a0 = 0;
  np->parent = p;

  // Copy open file descriptors
  for (int i = 0; i < 16; i++) {
    if (p->ofile[i]) {
      np->ofile[i] = p->ofile[i];
      np->ofile[i]->ref_count++;
    }
  }

  np->state = RUNNABLE;
  release(&np->lock);

  LOG_TRACE("Forked process %d", np->pid);

  return np->pid;
}

int exit() {
  struct Process *current;
  struct Process *p;

  current = get_proc();
  for (int i = 0; i < NPROC; i++) {
    p = &proc[i];
    acquire(&p->lock);
    if (p->parent == current) {
      p->parent = &proc[0];
    }
    release(&p->lock);
  }

  wakeup(current->parent);

  acquire(&current->lock);
  current->state = ZOMBIE;

  LOG_TRACE("Process %d exited", current->pid);

  sched();

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
    for (int i = 0; i < NPROC; i++) {
      p = &proc[i];
      acquire(&p->lock);
      if (p->parent == cur) {
        havekids++;
        if (p->state == ZOMBIE) {
          child_pid = p->pid;
          release(&p->lock);

          freeproc(p);
          return child_pid;
        }
      }
      release(&p->lock);
    }
    if (havekids == 0) {
      return -1;
    }
    sleep(cur, &cur->lock);
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
    acquire(&cur->lock);
    if (!uvmdealloc(cur->pagetable, old_head_top, size))
      return 0;
    release(&cur->lock);
  }

  acquire(&cur->lock);
  cur->heap_top = new_head_top;
  release(&cur->lock);
  LOG_TRACE("sbrk: success");
  return old_head_top;
}
