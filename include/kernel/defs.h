#ifndef DEFS_H
#define DEFS_H

#include "kernel/types.h"

struct spinlock;
struct buf;

// proc.c
int cpuid();
struct cpu *get_cpu();
struct Process *get_proc();
void init_proc();

// spinlock.c
void initlock(struct spinlock *lk, char *name);
int holding(struct spinlock *lk);
void push_off(void);
void pop_off(void);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
void sleep(void *chan, struct spinlock *lk);
void wakeup(void *chan);

// sleeplock.c
struct sleeplock;
void initsleeplock(struct sleeplock *lock, char *name);
void acquiresleep(struct sleeplock *lk);
void releasesleep(struct sleeplock *lk);
int holdingsleep(struct sleeplock *lk);

// kalloc.c
void kalloc_init();
void kfree(void *va);
void *kalloc();
void *ekalloc();

// printf.c
void kputc(char c);
void kputs(const char *s);
void kprintf(const char *fmt, ...);
void _panic(const char *file, int line, const char *fmt, ...);

// string.c
void *memset(void *s, int c, long n);
void *memcpy(void *dest, const void *src, long n);
void *memmove(void *dest, const void *src, long n);
void strcpy(char *dst, const char *src);
int strcmp(const char *s1, const char *s2);
long strlen(const char *str);

// syscall.c
void argint(int n, int *ip);
void argaddr(int n, uint64 *ip);
int argstr(int n, char *buf, int max);
void syscall();

// exec.c
int exec(char *path);

// proc.c
struct file;

void user_init();
void procinit(void);
void scheduler(void);
void sched(void);
void yield(void);
int alloc_fd(struct Process *p, struct file *f);
int file_alloc();
int fork();
int exit();
int wait();
uint64 sbrk(int64);

// file.c
int open(const char *path, int flags);
int dup(int fd);
int filestat(int fd, uint64 user_st_addr);

// vfs.c
void vfs_init();
struct vfs_inode *dirlookup(struct vfs_inode *ip, char *name);
struct vfs_inode *vfs_lookup(struct vfs_inode *node, char *path);
char *skipelem(char *path, char *name);
void test_vfs();
struct super_block *mount_easyfs();

// fs.c
struct vfs_inode *namei(char *path);
uint readi(struct vfs_inode *ip, int user_dst, uint64 dst, uint32 off,
	   uint32 size);
void ilock(struct vfs_inode *ip);
void iunlock(struct vfs_inode *ip);

// bcache.c
void bwrite(struct buf *buffer);
struct buf *bread(int dev, uint64 blockno);
void brelse(struct buf *b);
struct buf *bget(uint32 dev, uint64 blkno);
void binit(void);
uint bmap(struct vfs_inode *ip, uint32 block_num);
void bfree(uint32 block_num);
uint32 balloc();

// icache.c
struct vfs_inode *get_inode(uint32 ino);
void icache_init(void);
void put_inode(struct vfs_inode *t);
void iupdate(struct vfs_inode *ip);

// virtio_disk.c
void virtio_disk_init();
void virtio_disk_rw(struct buf *buffer, int write);
void virtio_disk_intr();
void test_virtio_disk();

void test_read_img();
void binit(void);
#endif
