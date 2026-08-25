
#define LOG_MODULE "TTY"

#include "kernel/proc.h"
#include "driver/arch/console.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"
#include "kernel/spinlock.h"
#include "kernel/syscall.h"

struct terminal {
	struct Process *input_owner[NPROC];
	int input_owner_idx; // -1 is empty
};

static struct terminal tty = {.input_owner = {0}, .input_owner_idx = -1};
static struct spinlock tty_owner_lock = {
    .name = "tty_owner", .locked = 0, .cpu = 0};

void terminal_claim_input(int pid)
{
	extern struct Process proc[NPROC];

	acquire(&tty_owner_lock);
	for (int i = 0; i < NPROC; i++) {
		struct Process *p = &proc[i];
		acquire(&p->lock);
		if (p->pid == pid && p->state != UNUSED) {
			for (int j = 0; j <= tty.input_owner_idx; j++) {
				if (tty.input_owner[j] == p) {
					release(&p->lock);
					release(&tty_owner_lock);
					return;
				}
			}
			if (tty.input_owner_idx == NPROC - 1) {
				LOG_WARN("terminal_claim_input: too many input "
					 "owners");
				release(&p->lock);
				release(&tty_owner_lock);
				return;
			}
			tty.input_owner[++tty.input_owner_idx] = p;
			release(&p->lock);
			release(&tty_owner_lock);
			return;
		}
		release(&p->lock);
	}
	release(&tty_owner_lock);
}

uint64 sys_terminal_claim_input(void)
{
	int pid;
	argint(ARG0, &pid);
	terminal_claim_input(pid);
	return 0;
}

void terminal_release_input(void)
{
	acquire(&tty_owner_lock);
	int cnt = tty.input_owner_idx;
	if (cnt == -1) {
		release(&tty_owner_lock);
		return;
	}

	for (int i = 0; i <= cnt; i++) {
		struct Process *p = tty.input_owner[i];
		if (p == 0) {
			continue;
		}
		acquire(&p->lock);
		signal_send(p, SIGINT);
		release(&p->lock);
	}

	tty.input_owner_idx = -1;
	release(&tty_owner_lock);
}

void terminal_forget_process(struct Process *p)
{
	acquire(&tty_owner_lock);
	for (int i = 0; i <= tty.input_owner_idx; i++) {
		if (tty.input_owner[i] != p)
			continue;

		for (int j = i; j < tty.input_owner_idx; j++)
			tty.input_owner[j] = tty.input_owner[j + 1];
		tty.input_owner[tty.input_owner_idx--] = 0;
		release(&tty_owner_lock);
		return;
	}
	release(&tty_owner_lock);
}

struct spinlock tty_lock = {.name = "tty_lock", .locked = 0, .cpu = 0};

static int devtmpfs_tty_write(struct file *, uint8 *buffer, uint32 size)
{
	acquire(&tty_lock);
	for (uint32 i = 0; i < size; i++) {
		arch_console_putc(buffer[i]);
	}
	release(&tty_lock);
	return (int) size;
}

static int devtmpfs_tty_read(struct file *, uint8 *buffer, uint32 size)
{
	if (size == 0)
		return 0;

	int c;
	while ((c = arch_console_getc()) <= 0) {
		yield();
	}
	buffer[0] = (uint8) c;
	return 1;
}

struct vfs_file_ops tty_ops = {
    .read = devtmpfs_tty_read,
    .write = devtmpfs_tty_write,
};
