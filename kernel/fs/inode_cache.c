#include "kernel/defs.h"
#include "kernel/icache.h"
#include "kernel/log.h"
#include "kernel/types.h"

struct inode_cache icache;

/**
 * icache_init - initialize the inode cache
 * */
void icache_init(void) {

  initlock(&icache.lock, "icache lock");

  struct vfs_inode *inc;
  // Create linked list of buffers
  icache.head.prev = &icache.head;
  icache.head.next = &icache.head;
  for (inc = icache.inodes; inc < icache.inodes + NINODES; inc++) {
    inc->next = icache.head.next;
    inc->prev = &icache.head;
    inc->count = 0;
    initlock(&inc->lock, "inode lock");
    icache.head.next->prev = inc;
    icache.head.next = inc;
  }
}

/**
 * get_inode - search ino in the inode cache
 *
 * Return: pointer to the inode
 * */
struct vfs_inode *get_inode(uint32 ino) {
  struct vfs_inode *t;
  acquire(&icache.lock);

  // Check if the pointer survived until here
  for (t = &icache.inodes[0]; t < &icache.inodes[NINODES]; t++) {
    if (t->ino == ino && t->count > 0) {
      t->count++;
      release(&icache.lock);
      return t;
    }
  }

  // LRU need start from the tail
  for (t = icache.head.prev; t != &icache.head; t = t->prev) {
    if (t->count == 0) {
      t->ino = ino;
      t->count = 1;
      release(&icache.lock);
      return t;
    }
  }

  LOG_WARN("get_inode: no inodes");

  release(&icache.lock);
  return 0;
}

/**
 * put_inode - put inode into the inode cache
 * */
void put_inode(struct vfs_inode *t) {
  acquire(&icache.lock);
  if (t->count == 1){
    t->count = 0; 
    // Move this recently freed inode to the MRU position (head.next)
    t->prev->next = t->next;
    t->next->prev = t->prev;
    
    t->next = icache.head.next;
    t->prev = &icache.head;
    icache.head.next->prev = t;
    icache.head.next = t;
  }else{
    t->count--;
  }
  release(&icache.lock);
}
