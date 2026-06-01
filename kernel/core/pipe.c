#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"

#define LOG_MODULE "PIPE"

int pipe_alloc(struct file **read, struct file **write)
{
	struct pipe *pi = 0;
	*read = filealloc();
	*write = filealloc();
	if (*read == 0 || *write == 0) {
		goto fail;
	}
	if ((pi = (struct pipe *) kalloc()) == 0) {
		LOG_WARN("pipe_alloc: kalloc failed");
		goto fail;
	}
	pi->readable = 1;
	pi->writable = 1;
	pi->nread = 0;
	pi->nwrite = 0;
	initlock(&pi->lock, "pipe");

	(*read)->pipe = pi;
	(*read)->type = FILE_PIPE;
	(*read)->readable = 1;
	(*read)->writable = 0;

	(*write)->pipe = pi;
	(*write)->type = FILE_PIPE;
	(*write)->writable = 1;
	(*write)->readable = 0;

	return 0;

fail:
	if (pi) {
		kfree(pi);
	}
	if (*read) {
		fileclose(*read);
	}
	if (*write) {
		fileclose(*write);
	}
	return -1;
}

void pipe_close(struct pipe *pi, int writable)
{
	acquire(&pi->lock);
	if (writable) {
		pi->writable = 0;
		wakeup(&pi->nread);
	} else {
		pi->readable = 0;
		wakeup(&pi->nwrite);
	}
	if (pi->readable == 0 && pi->writable == 0) {
		release(&pi->lock);
		kfree(pi);
	} else {
		release(&pi->lock);
	}
}

int pipe_read(struct pipe *pi, uint8 *buffer, uint32 size)
{
	int i;
	char ch = 0;
	struct Process *p = get_proc();

	acquire(&pi->lock);
	// wait until there is something to read
	while (pi->nread == pi->nwrite && pi->writable) {
		sleep(&pi->nread, &pi->lock);
	}
	for (i = 0; i < size; i++) {
		if (pi->nread == pi->nwrite) {
			break;
		}

		char ch = pi->buf[pi->nread % PIPE_BUF_SIZE];
		if (copyout(p->pagetable, (char *) buffer + i, (uint64) &ch,
			    1) < 0) {
			if (i == 0)
				i = -1;
			break;
		}
		pi->nread++;
	}

	wakeup(&pi->nwrite);
	release(&pi->lock);
	return i;
}

int pipe_write(struct pipe *pi, uint8 *buffer, uint32 size)
{
	int i = 0;
	char ch;
	struct Process *p = get_proc();

	acquire(&pi->lock);
	while (i < size) {
		// When readable is 0, it means the pipe is closed
		if (pi->readable == 0) {
			release(&pi->lock);
			return -1;
		}
		if (pi->nwrite - pi->nread >= PIPE_BUF_SIZE) {
			// wait until there is space to write
			wakeup(&pi->nread);
			sleep(&pi->nwrite, &pi->lock);
		} else {
			if (copyin(p->pagetable, &ch, (uint64) buffer + i, 1) <
			    0) {
				break;
			}
			pi->buf[pi->nwrite++ % PIPE_BUF_SIZE] = ch;
			i++;
		}
	}

	wakeup(&pi->nread);
	release(&pi->lock);
	return i;
}
