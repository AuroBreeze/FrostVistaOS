#include "asm/defs.h"
#include "core/proc.h"
#include "kernel/defs.h"
#include "kernel/fs.h"
#include "kernel/log.h"

#define LOG_MODULE "PIPE"

/**
 * pipe_alloc - allocate a new pipe and bind a read/write file pair to it
 *
 * Context: process context, called by sys_pipe.
 *   Allocates two file structs and a pipe ring buffer. The read-end file
 *   (@read) is marked readable-only, the write-end file (@write) writable-only.
 *   Both files reference the same backing pipe struct. On failure at any stage,
 *   the fail label auto-cleans up: partially-allocated files are closed via
 *   fileclose() and the pipe struct is freed via kfree(), ensuring no leaks.
 *
 * Return: 0 on success, -1 on allocation failure
 * */
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

/**
 * pipe_close - close one end of a pipe
 *
 * Context: process context, called by fileclose.
 *   If @writable, marks the pipe non-writable and wakes blocked readers;
 *   otherwise marks it non-readable and wakes blocked writers. When both
 *   ends are closed (readable == 0 && writable == 0), the pipe struct is
 *   freed. The caller must not reference the pipe after this call.
 *
 * Return: void
 * */
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

/**
 * pipe_read - read up to @size bytes from the pipe into user-space @buffer
 *
 * Context: process context, may block (sleep).
 *   Acquires the pipe lock. Sleeps on &pi->nread if empty but still writable.
 *   Reads byte-by-byte via copyout(), advancing nread around PIPE_BUF_SIZE.
 *   If copyout fails before any data was copied returns -1; otherwise stops
 *   and returns partial count. On return, wakes writers blocked on a full pipe.
 *
 * Return: bytes read, 0 if pipe empty and write end closed,
 *         -1 on copyout failure before any byte
 * */
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

/**
 * pipe_write - write up to @size bytes from user-space @buffer into the pipe
 *
 * Context: process context, may block (sleep).
 *   Acquires the pipe lock. Returns -1 immediately if the read end is closed
 *   (readable == 0). If the buffer is full (nwrite - nread >= PIPE_BUF_SIZE),
 *   wakes readers then sleeps on &pi->nwrite. Copies bytes one at a time via
 *   copyin(), advancing nwrite around PIPE_BUF_SIZE. If copyin fails, stops
 *   early and returns partial count. On return, wakes readers.
 *
 * Return: bytes written, 0 if copyin fails before any byte,
 *         -1 if the read end is closed
 * */
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
				// If copyin fails before any byte, return -1
				if (i == 0)
					i = -1;
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
