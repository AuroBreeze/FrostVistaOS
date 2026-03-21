#include "asm/defs.h"
#include "asm/mm.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/elf.h"
#include "kernel/init_code.h"
#include "kernel/log.h"
#include "kernel/types.h"

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

int flags2perm(int flags) {
  int perm = 0;
  if (flags & 0x1)
    perm |= PTE_X;
  if (flags & 0x2)
    perm |= PTE_W;
  if (flags & 0x4)
    perm |= PTE_R;
  return perm;
}

/**
 * loadseg - Load a segment into pagetable
 * */
static int loadseg(pagetable_t pagetable, uint64 va, uint8 *src, uint64 size) {
  uint64 i = 0;
  while (i < size) {
    uint64 va_page = PGROUNDDOWN(va + i);
    uint64 offset = (va + i) - va_page;

    uint64 pa = walk_addr(pagetable, va_page);
    if (pa == 0) {
      panic("loadseg: walk failed");
    }

    uint64 n = PGSIZE - offset;
    if (n > size - i) {
      n = size - i;
    }

    memcpy((void *)(PA2VA(pa) + offset), src + i, n);
    i += n;
  }

  return 1;
}

int exec() {
  uint64 va_start, va_end;
  extern struct Process *current_proc;

  struct elfhdr *eh = (struct elfhdr *)init_elf;
  if (eh->magic != ELF_MAGIC) {
    LOG_WARN("exec: magic number is not ELF_MAGIC");
    return 0;
  }
  pagetable_t user_pagetable = create_user_pagetable();

  int i, off;
  for (i = 0, off = eh->phoff; i < eh->phnum;
       i++, off += sizeof(struct proghdr)) {
    struct proghdr *ph = (struct proghdr *)(init_elf + off);
    if (ph->type != ELF_PROG_LOAD)
      continue;

    va_start = ph->vaddr;
    va_end = va_start + ph->memsz;

    if (!uvmalloc(user_pagetable, va_start, va_end - va_start,
                  flags2perm(ph->flags))) {
      // Clean up
      uvmdealloc(user_pagetable, va_start, va_end - va_start);
      return 0;
    }

    loadseg(user_pagetable, va_start, init_elf + ph->off, ph->filesz);
  }

  uint64 sz = PGROUNDUP(va_end);

  // Protection Page
  if (!uvmalloc(user_pagetable, sz, PGSIZE, 0)) {
    uvmfree(user_pagetable, current_proc);
    LOG_WARN("uvmalloc failed");
    return 0;
  }

  uint64 new_heap_bottom = sz + PGSIZE;
  uint64 new_heap_top = sz + PGSIZE;
  uint64 user_stack_top = PHYSTOP_LOW;
  uint64 user_stack_bottom = PHYSTOP_LOW - PGSIZE;

  if (!uvmalloc(user_pagetable, user_stack_bottom, PGSIZE, PTE_R | PTE_W)) {
    uvmdealloc(user_pagetable, 0, new_heap_top);
    return 0;
  }

  struct Process new_layout;
  new_layout.heap_top = new_heap_top;
  new_layout.stack_bottom = user_stack_bottom;
  new_layout.stack_top = user_stack_top;

  // Simulated shell
  char *args[] = {"init", "hello", "word", 0};
  int argc = 3;
  uint64 ustack[3 + 1];

  uint64 sp = user_stack_top;

  for (int i = 0; i < argc; i++) {
    int len = strlen(args[i]) + 1;
    sp -= len;
    if (!copyout(user_pagetable, (char *)sp, (uint64)args[i], len)) {
      uvmfree(user_pagetable, &new_layout);
      return 0;
    }
    ustack[i] = sp;
  }

  ustack[argc] = 0;
  sp = sp & ~0x0F;

  int array_size = (argc + 1) * sizeof(uint64);
  sp -= array_size;

  if (!copyout(user_pagetable, (char *)sp, (uint64)ustack, array_size)) {
    uvmfree(user_pagetable, &new_layout);
    return 0;
  }

  pagetable_t old_pagetable = current_proc->pagetable;
  struct Process *old_process = (struct Process *)kalloc();
  *old_process = *current_proc;

  current_proc->pagetable = user_pagetable;
  current_proc->heap_bottom = new_heap_bottom;
  current_proc->heap_top = new_heap_top;
  current_proc->stack_bottom = user_stack_bottom;
  current_proc->stack_top = user_stack_top;

  current_proc->trapframe->sp = sp;
  current_proc->trapframe->a0 = argc;
  current_proc->trapframe->a1 = sp;
  current_proc->trapframe->epc = eh->entry;

  if (old_process->heap_top > 0) {
    uvmfree(old_pagetable, old_process);
    kfree(old_process);
  }

  LOG_TRACE("exec: program loaded to 0x%x", eh->entry);
  return 1;
}
