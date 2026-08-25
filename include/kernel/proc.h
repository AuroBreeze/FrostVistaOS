#ifndef __KERNEL_PROC_H
#define __KERNEL_PROC_H

#include "kernel/arch.h"
#include "kernel/fs.h"
#include "kernel/types.h"
#include "kernel/vma.h"
#include "kernel/signal.h"

#define NPROC 64
#define NCPU 16

// wai4 options
#define WNOHANG 1

// NOTE: Increasing NOFILE grows struct Process. Keep large Process copies out
// of the 4KB kernel stack; exec once hung when it copied struct Process after
// this value was raised for Linux ABI tests such as dup2(fd, 100).
#define NOFILE 128

// Per-CPU state.
struct cpu {
	struct Process *proc;	// The process running on this cpu, or null.
	arch_context_t context; // swtch() here to enter scheduler().
	int noff;		// Record nesting depth
	int intena; // Record the interrupt status before the first interrupt is
		    // disabled
};

enum proc_state { UNUSED, USED, RUNNABLE, RUNNING, SLEEPING, ZOMBIE };

struct Process {
	enum proc_state state;
	struct spinlock lock;	    // Lock to protect the process
	void *chan;		    // wakeup channel
	int pid;		    // Process ID
	char name[16];		    // Process name
	struct file *ofile[NOFILE]; // Open files
	char cwd[PATH_MAX];	    // Current working directory
	int exit_code;		    // Exit code

	uint64 kstack;		     // Kernel stack pointer
	struct Process *parent;	     // Parent process
	pagetable_t pagetable;	     // Page table
	arch_context_t *context;     // Kernel context
	arch_trapframe_t *trapframe; // User trap frame

	uint64 size;	    // Size of process memory but remove from the stack
	uint64 heap_bottom; // Low address
	uint64 heap_top;    // High address

	uint64 stack_bottom; // Low address
	uint64
	    stack_top; // Upper boundary in the pagetable, Usually PHYSTOP_LOW

	struct vm_area_struct vm_area[NVMA]; // Virtual memory areas
	struct sighand sighand;		     // Per-process signal state
};

extern int pid;

#endif
