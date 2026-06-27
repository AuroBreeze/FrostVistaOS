#include "driver/hal_console.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/spinlock.h"

struct spinlock tty_lock = {.name = "tty_lock", .locked = 0, .cpu = 0};

static int devtmpfs_tty_write(struct file *, uint8 *buffer, uint32 size)
{
	acquire(&tty_lock);
	for (uint32 i = 0; i < size; i++) {
		hal_console_putc(buffer[i]);
	}
	release(&tty_lock);
	return (int) size;
}

static int devtmpfs_tty_read(struct file *, uint8 *buffer, uint32 size)
{
	if (size == 0)
		return 0;

	int c;
	while ((c = hal_console_getc()) <= 0) {
		yield();
	}

	buffer[0] = (uint8) c;
	return 1;
}

struct vfs_file_ops tty_ops = {
    .read = devtmpfs_tty_read,
    .write = devtmpfs_tty_write,
};
