#include "kernel/sleeplock.h"
#include "core/proc.h"
#include "kernel/defs.h"

void initsleeplock(struct sleeplock *lock, char *name) {
  initlock(&lock->lock, "sleep lock");
  lock->name = name;
  lock->pid = 0;
  lock->locked = 0;
}

void acquiresleep(struct sleeplock *lk) {
  acquire(&lk->lock);
  while (lk->locked) {
    sleep(lk, &lk->lock);
  }
  lk->locked = 1;
  lk->pid = get_proc()->pid;
  release(&lk->lock);
}

void releasesleep(struct sleeplock *lk) {
  acquire(&lk->lock);
  lk->locked = 0;
  lk->pid = 0;
  wakeup(lk);
  release(&lk->lock);
}

int holdingsleep(struct sleeplock *lk) {
  int r;
  acquire(&lk->lock);
  // Does the current running instance hold a lock
  r = lk->locked && lk->pid == get_proc()->pid;
  release(&lk->lock);
  return r;
}
