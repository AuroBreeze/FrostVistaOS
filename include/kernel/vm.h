#ifndef __KERNEL_VM_H__
#define __KERNEL_VM_H__

/*
 * Architecture-independent page mapping permissions.
 * These values are translated to hardware PTE bits by each architecture.
 */
#define PTE_READ (1ULL << 0)
#define PTE_WRITE (1ULL << 1)
#define PTE_EXEC (1ULL << 2)
#define PTE_USER (1ULL << 3)
#define PTE_VALID (1ULL << 4)

#endif
