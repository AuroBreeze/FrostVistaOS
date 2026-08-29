
#define LOG_MODULE "PROC"

#include "kernel/mm/kmalloc.h"
#include "kernel/arch/irq.h"
#include "kernel/arch/cpu.h"
#include "kernel/arch/trap.h"
#include "kernel/arch/user.h"
#include "kernel/signal.h"

#include "kernel/arch/vm.h"
#include "kernel/vm.h"
#include "kernel/proc.h"
#include "kernel/arch/mm.h"
#include "kernel/defs.h"
#include "kernel/string.h"
#include "kernel/fcntl.h"
#include "kernel/log.h"
#include "kernel/spinlock.h"

#define NFILE 128

struct file ftable[NFILE];
struct spinlock ftable_lock = {.name = "ftable_lock", .locked = 0, .cpu = 0};
extern pagetable_t kernel_table;

int fd_alloc()
{
	for (int i = 0; i < NFILE; i++) {
		if (ftable[i].ref_count == 0) {
			return i;
		}
	}
	return -1;
}

/**
 * alloc_process - Allocate a process
 *
 * Context:
 *
 * Return: if process allocated, return a pointer to the process, else return 0
 * */
struct Process *alloc_process(void)
{
	struct Process *p;
	for (p = proc; p < &proc[NPROC]; p++) {
		acquire(&p->lock);
		if (p->state == UNUSED) {
			p->state = USED;
			p->pid = get_pid();
			release(&p->lock);
			p->kstack = (uint64) kalloc();
			p->pagetable = uvmcreate();

			if (p->kstack == 0 || p->pagetable == 0) {
				panic(
				    "Alloc process: Failed to allocate memory");
			}

			// NOTE:
			// Position the trapframe above the stack, that is, at a
			// lower address in order to store data in the trapframe
			p->trapframe =
			    (arch_trapframe_t *) (p->kstack + PGSIZE -
						  sizeof(arch_trapframe_t));

			extern void usertrapret(void);
			// NOTE: p->context must be allocated in the kernel
			// otherwise it will panic
			p->context =
			    (arch_context_t *) kmalloc(sizeof(arch_context_t));
			if (p->context == 0) {
				panic(
				    "Alloc process: Failed to allocate memory");
			}
			p->context->ra = (uint64) usertrapret;

			memset((void *) &p->sighand, 0, sizeof(struct sighand));
			for (int i = 0; i < NOFILE; i++) {
				p->ofile[i] = 0;
			}
			strcpy(p->cwd, "/");

			// NOTE:
			// Point sp to a location not used by the trapframe
			p->context->sp =
			    (uint64) (p->trapframe) - sizeof(arch_context_t);
			return p;
		}
		release(&p->lock);
	}
	return 0;
}

void first_ret()
{
	struct Process *p = get_proc();
	release(&p->lock);

	// Install the final root filesystem after block devices and caches are
	// ready, then reattach device files under that root.
#ifdef ROOTFS_EXT4
	extern int ext4_mount_root(uint32 dev);
	ext4_mount_root(0);
#else
	extern int easyfs_mount_root();
	easyfs_mount_root();
#endif

#ifdef CONFIG_FS_DEVTMPFS
	extern struct vfs_inode *devtmpfs_root();
	vfs_mount_fs("/dev", devtmpfs_root());
#endif

#ifdef CONFIG_FS_TMPFS
	extern struct vfs_inode *tmpfs_root();
	vfs_mount_fs("/tmp", tmpfs_root());
	// extern void tmpfs_test(void);
	// tmpfs_test();
#endif

#ifdef CONFIG_FS_EXT4
	// extern void mix_test(void);
	// mix_test();
#endif

	extern void usertrapret(void);
	usertrapret();
}

void user_init()
{
	LOG_TRACE("Initializing user process");
	struct Process *p = alloc_process();
	// NOTE: The use of spin locks requires
	// processes running on the CPU.
	if (p == 0) {
		panic("Failed to allocate process");
	}

	uint64 user_code_table = (uint64) kalloc();
	if (user_code_table == 0) {
		panic("Failed to allocate memory");
	}
	uint64 user_stack = (uint64) kalloc();
	if (user_stack == 0) {
		panic("Failed to allocate memory");
	}

	const uint8 *user_code = arch_user_init_code();
	uint64 user_code_size = arch_user_init_code_size();
	if (user_code == 0 || user_code_size == 0 || user_code_size > PGSIZE)
		panic("Initial user image is not supported on this architecture");

	memcpy((uint64 *) user_code_table, user_code, user_code_size);

	kvmmap(p->pagetable, 0x0, arch_kva_to_pa(user_code_table),
	       PGSIZE, PTE_USER | PTE_READ | PTE_WRITE | PTE_EXEC);
	uint64 user_stack_va = 0x40000;
	kvmmap(p->pagetable, user_stack_va, arch_kva_to_pa(user_stack),
	       PGSIZE, PTE_USER | PTE_READ | PTE_WRITE);

	uint64 user_stack_top = user_stack_va + PGSIZE;
	p->trapframe->sp = user_stack_top;
	p->trapframe->arch_epc = 0x0;
	p->context->ra = (uint64) first_ret;

	struct cpu *c = get_cpu();
	// NOTE: set the current process that allow spinlock can work
	c->proc = p;

	int fd0 = open("/dev/tty", O_RDONLY); // stdin
	int fd1 = open("/dev/tty", O_WRONLY); // stdout
	int fd2 = dup(fd1);		      // stderr

	if (fd0 < 0 || fd1 < 0 || fd2 < 0) {
		LOG_TRACE("fd0: %d, fd1: %d, fd2: %d", fd0, fd1, fd2);
		panic("Failed to open files");
	}

	// NOTE: Clear the CPU processes in the settings
	c->proc = 0;

	p->state = RUNNABLE;
	LOG_TRACE("User process initialized");
}

void scheduler(void)
{
	struct Process *p;
	extern void swtch(arch_context_t * old, arch_context_t * new);

	for (;;) {
		arch_irq_enable();
		int found = 0;
		for (p = proc; p < &proc[NPROC]; p++) {
			acquire(&p->lock);
			if (p->state == RUNNABLE) {
				p->state = RUNNING;
				found = 1;
				LOG_TRACE("Switching to process %d", p->pid);

				struct Process *myproc = p;
				struct cpu *c = get_cpu();
				c->proc = myproc;

				arch_trapframe_t *trapframe = myproc->trapframe;

				trapframe = p->trapframe;

				// Because in uservec, addi sp, sp, -256 is
				// first used, uservec can properly align with
				// the trapframe and store data into it.
				w_sscratch(p->kstack + PGSIZE);

				w_satp(MAKE_SATP(
				    arch_kva_to_pa((uint64) p->pagetable)));
				sfence_vma();

				swtch(&c->context, p->context);

				c->proc = 0;

				// Ensure that the value written to the register
				// is the actual physical address
				w_satp(MAKE_SATP(
				    arch_kva_to_pa((uint64) kernel_table)));
				sfence_vma();

				LOG_TRACE("Switched back to kernel");
			}
			// The lock will be reacquired in the `yield` block
			// So what is actually being released here is the lock
			// added by `yield` or `swtch`.
			release(&p->lock);
		}
		if (!found) {
			arch_cpu_wait();
		}
	}
	LOG_TRACE("Scheduler done");
}

/**
 * alloc_fd - Allocate a free file descriptor
 * */
int alloc_fd(struct Process *p, struct file *f)
{
	acquire(&p->lock);
	for (int i = 0; i < NOFILE; i++) {
		if (p->ofile[i] == 0) {
			p->ofile[i] = f;
			release(&p->lock);
			return i;
		}
	}
	release(&p->lock);
	return -1;
}

void freeproc(struct Process *p)
{
	acquire(&p->lock);
	p->pid = 0;
	p->name[0] = 0;

	if (p->kstack) {
		kfree((void *) p->kstack);
		p->kstack = 0;
	}

	if (p->pagetable) {
		uvmfree(p->pagetable, p);
		p->pagetable = 0;
	}

	if (p->context) {
		kmfree((void *) p->context);
		p->context = 0;
	}
	p->trapframe = 0;
	p->size = 0;
	p->parent = 0;

	p->state = UNUSED;
	release(&p->lock);
}

int fork()
{
	LOG_TRACE("Forking");
	struct Process *np = alloc_process();
	struct Process *p = get_proc();
	if (np == 0) {
		return -1;
	}

	acquire(&np->lock);
	if (uvmcopy(p->pagetable, np->pagetable) < 0) {
		release(&np->lock);
		freeproc(np);
		return -1;
	}

	np->heap_top = p->heap_top;
	np->heap_bottom = p->heap_bottom;
	np->stack_top = p->stack_top;
	np->stack_bottom = p->stack_bottom;
	strcpy(np->cwd, p->cwd);
	LOG_DEBUG("fork: parent pid=%d cwd=\"%s\" child pid=%d cwd=\"%s\"",
		  p->pid, p->cwd, np->pid, np->cwd);
	LOG_DEBUG("fork: np=%p p=%p np_cwd=%p p_cwd=%p", (void *) np,
		  (void *) p, (void *) np->cwd, (void *) p->cwd);

	*(np->trapframe) = *(p->trapframe);
	np->trapframe->a0 = 0;
	np->parent = p;
	np->sighand = p->sighand;

	// Copy open file descriptors
	for (int i = 0; i < NOFILE; i++) {
		if (p->ofile[i]) {
			np->ofile[i] = filedup(p->ofile[i]);
		}
	}

	// copy VM areas
	for (int i = 0; i < NVMA; i++) {
		if (p->vm_area[i].used == 1) {
			np->vm_area[i] = p->vm_area[i];
			if (p->vm_area[i].file != 0) {
				np->vm_area[i].file =
				    filedup(p->vm_area[i].file);
			}
		}
	}

	np->state = RUNNABLE;
	release(&np->lock);

	LOG_TRACE("Forked process %d", np->pid);

	return np->pid;
}

int exit(int exit_code)
{
	struct Process *current;
	struct Process *p;

	current = get_proc();
	terminal_forget_process(current);

	for (int i = 0; i < NOFILE; i++) {
		if (current->ofile[i] != 0) {
			struct file *f = current->ofile[i];
			current->ofile[i] = 0;
			fileclose(f);
		}
	}

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
	current->exit_code = exit_code;
	for (int i = 0; i < NVMA; i++) {
		struct vm_area_struct *vma = &current->vm_area[i];
		if (vma->used == 0)
			continue;

		uint64 len = vma->va_end - vma->va_start;
		kvmunmap(current->pagetable, vma->va_start, len, 1);
		if (vma->file != 0) {
			fileclose(vma->file);
		}
		vma->used = 0;
	}

	LOG_TRACE("Process %d exited", current->pid);

	sched();

	panic("zombie exit: return from swtch");

	return 0;
}

/**
 * wait4 - wait for a matching child process to exit
 *
 * pid: -1 waits for any child, otherwise waits for the child with this pid
 * wstatus: user-space address to store the encoded exit status, or 0 to ignore
 * options: 0 waits until exit, WNOHANG returns immediately if no child exited
 *
 * Return: child pid on success, 0 for WNOHANG with a live matching child,
 *         -1 if no matching child exists or status copyout fails
 */
uint64 wait4(int pid, uint64 wstatus, int options)
{
	struct Process *cur = get_proc();
	int have_match;
	int child_pid;

	acquire(&cur->lock); // Hold the parent process lock to prevent missing
			     // the wakeup call when the child process exits

	for (;;) {
		have_match = 0;
		for (int i = 0; i < NPROC; i++) {
			struct Process *p = &proc[i];

			// If it were me, I'd just skip this step, since we
			// already hold cur->lock
			if (p == cur)
				continue;

			acquire(&p->lock);
			if (p->parent == cur && (pid == -1 || p->pid == pid)) {
				// A matching child exists even if it has not
				// exited yet. Otherwise waitpid(pid, ..., 0)
				// would report no child instead of sleeping
				// until the child becomes ZOMBIE.
				have_match = 1;
				if (p->state == ZOMBIE) {
					child_pid = p->pid;
					if (wstatus != 0) {
						int status =
						    (p->exit_code & 0xff) << 8;
						if (copyout(cur->pagetable,
							    (char *) wstatus,
							    (uint64) &status,
							    sizeof(status)) <
						    0) {
							release(&p->lock);
							release(&cur->lock);
							return -1;
						}
					}
					release(&p->lock);
					release(&cur->lock);

					freeproc(p);
					return child_pid;
				}
			}
			release(&p->lock);
		}

		if (have_match == 0) {
			release(&cur->lock);
			return -1;
		}

		if (options & WNOHANG) {
			release(&cur->lock);
			return 0;
		}

		// If you enter `sleep` while holding `cur->lock`, `sleep` will
		// release it internally and enter the scheduler.
		sleep(cur, &cur->lock);
	}
}

uint64 brk(uint64 addr)
{
	struct Process *cur = get_proc();
	uint64 old_head_top = cur->heap_top;
	uint64 new_head_top = addr;

	LOG_TRACE("brk: old_head_top %p, new_head_top %p",
		  (void *) old_head_top, (void *) addr);

	if (addr == 0) {
		return old_head_top;
	}

	if (addr < cur->heap_bottom || addr >= cur->stack_bottom) {
		return old_head_top;
	}

	if (addr >= cur->heap_bottom && addr < cur->heap_top) {
		acquire(&cur->lock);
		uvmdealloc(cur->pagetable, addr, old_head_top - addr);
		release(&cur->lock);
	}

	acquire(&cur->lock);
	cur->heap_top = addr;
	new_head_top = addr;
	release(&cur->lock);

	LOG_TRACE("brk: success");

	return new_head_top;
}

int kill(int pid, int sig)
{
	if (sig < 0 || sig >= NSIG)
		return -1;

	struct Process *p = 0;
	for (int i = 0; i < NPROC; i++) {
		acquire(&proc[i].lock);
		if (proc[i].pid == pid && proc[i].state != UNUSED) {
			p = &proc[i];
			break;
		}
		release(&proc[i].lock);
	}
	if (p == 0)
		return -1;
	if (sig == 0) {
		release(&p->lock);
		return 0;
	}

	signal_send(p, sig);

	release(&p->lock);
	return 0;
}
