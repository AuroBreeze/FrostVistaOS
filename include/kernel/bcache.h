#ifndef __KERNEL_BCACHE_H__
#define __KERNEL_BCACHE_H__

#include "kernel/sleeplock.h"
#include "kernel/spinlock.h"
#include "kernel/types.h"

#define BSIZE 1024
#define NNUM 32

struct buf {
  uint8 data[BSIZE];
  uint32 dev;
  int dirty; // Need to update the disk
  int valid;
  uint64 blkno;
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

#endif
