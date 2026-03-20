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
  struct elfhdr *eh = (struct elfhdr *)init;
  if (eh->magic != ELF_MAGIC) {
    LOG_WARN("exec: magic number is not ELF_MAGIC");
    return 0;
  }
  pagetable_t user_pagetable = create_user_pagetable();
  uint64 max_va = 0;

  int i, off;
  for (i = 0, off = eh->phoff; i < eh->phnum;
       i++, off += sizeof(struct proghdr)) {
    struct proghdr *ph = (struct proghdr *)(init + off);
    if (ph->type != ELF_PROG_LOAD)
      continue;

    uint64 va_start = ph->vaddr;
    uint64 va_end = va_start + ph->memsz;

    if (!uvmalloc(user_pagetable, va_start, va_end - va_start,
                  flags2perm(ph->flags))) {
      return 0;
    }

    loadseg(user_pagetable, va_start, init + ph->off, ph->filesz);

    if (va_end > max_va) {
      max_va = va_end;
    }
  }

  uint64 user_stack_va = 0x40000;
  if (max_va > user_stack_va) {
    user_stack_va = max_va;
  }
  if (!uvmalloc(user_pagetable, user_stack_va, PGSIZE, PTE_R | PTE_W)) {
    uvmfree(user_pagetable, PGSIZE);
    return 0;
  }

  uint64 sp = user_stack_va + PGSIZE;
  // Simulated shell
  char *args[] = {"init", "hello", "word", 0};
  int argc = 3;
  uint64 ustack[3 + 1];

  for (int i = 0; i < argc; i++) {
    int len = strlen(args[i]) + 1;
    sp -= len;
    if (!copyout(user_pagetable, (char *)sp, (uint64)args[i], len)) {
      uvmfree(user_pagetable, user_stack_va + PGSIZE);
      return 0;
    }
    ustack[i] = sp;
  }

  ustack[argc] = 0;
  sp = sp & ~0x0F;

  int array_size = (argc + 1) * sizeof(uint64);
  sp -= array_size;

  if (!copyout(user_pagetable, (char *)sp, (uint64)ustack, array_size)) {
    uvmfree(user_pagetable, user_stack_va + PGSIZE);
    return 0;
  }

  extern struct Process *current_proc;
  pagetable_t old_pagetable = current_proc->pagetable;
  uint64 old_size = current_proc->size;

  current_proc->pagetable = user_pagetable;
  current_proc->size = user_stack_va + PGSIZE;
  current_proc->trapframe->sp = sp;
  current_proc->trapframe->a0 = argc;
  current_proc->trapframe->a1 = sp;
  current_proc->trapframe->epc = eh->entry;

  if (old_size > 0) {
    uvmfree(old_pagetable, old_size);
  }

  LOG_TRACE("exec: program loaded to 0x%x", eh->entry);
  return 1;
}
