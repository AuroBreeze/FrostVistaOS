#ifndef FV_KERNEL_BCACHE_H
#define FV_KERNEL_BCACHE_H

#include "kernel/sleeplock.h"
#include "kernel/spinlock.h"
#include "kernel/types.h"

#define BSIZE 0x1000 // Block size 4096
#define NNUM 32

struct buf {
	uint8 data[BSIZE];
	uint32 dev;
	int dirty; // Need to update the disk
	int valid;
	uint64 blkno; // block num
	// Check if the read operation is complete
	int done;
	uint32 refcnt;
	uint32 flags;

	struct sleeplock buf_lock;

	// double linked list
	struct buf *next;
	struct buf *prev;
};

struct bcache {
	struct buf buf[NNUM];
	struct buf head; // A doubly linked list with a head node

	struct spinlock bcache_lock;
};

extern struct bcache bcache;

struct buf *bget(uint32 dev, uint64 blkno);
void brelse(struct buf *b);
struct buf *bread(int dev, uint64 blockno);
void bwrite(struct buf *buffer);

#endif
