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
const char *log_ts(void);

// string.c
void *memset(void *s, int c, long n);
void *memcpy(void *dest, const void *src, long n);
void *memmove(void *dest, const void *src, long n);
char *strncpy(char *s, const char *t, int n);
void strcpy(char *dst, const char *src);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *p, const char *q, uint n);
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
int fd_alloc();
int fork();
int exit();
int wait();
uint64 sbrk(int64);

// pipe.c
struct pipe;

int pipe_alloc(struct file **read, struct file **write);
void pipe_close(struct pipe *pi, int writable);
int pipe_read(struct pipe *pi, uint8 *buffer, uint32 size);
int pipe_write(struct pipe *pi, uint8 *buffer, uint32 size);

// file.c
int open(const char *path, int flags);
int openat(int dirfd, const char *path, int flags);
int mkdirat(int dirfd, const char *path, int mode);
int unlinkat(int dirfd, const char *path, int flags);
int dup(int fd);
int filestat(int fd, uint64 user_st_addr);
void fileclose(struct file *f);
struct file *filedup(struct file *f);
struct file *filealloc(void);

// vfs.c
void vfs_init();
struct vfs_inode *vfs_lookup_at(struct vfs_inode *node, char *path);
struct vfs_inode *vfs_create_at(struct vfs_inode *start, char *path, int type);
int vfs_mkdir_at(struct vfs_inode *dir, char *path, int flags);
struct vfs_inode *vfs_namei(char *path);
int vfs_mount_at(struct vfs_inode *parent, char *name, struct vfs_inode *root);
int vfs_mount_fs(char *path, struct vfs_inode *root);
void vfs_iput(struct vfs_inode *node);
int vfs_read_at(struct vfs_inode *node, uint64 off, uint8 *dst, uint32 size);
int vfs_unlink_at(struct vfs_inode *dir, char *path, int flags);
void vfs_ilock(struct vfs_inode *ip);
void vfs_iunlock(struct vfs_inode *ip);
char *skipelem(char *path, char *name);

// fs.c
int namecmp(const char *s, const char *t);

// bcache.c
void binit(void);

// icache.c
void icache_init(void);
struct vfs_inode *get_inode(uint32 dev, uint32 ino);
void put_inode(struct vfs_inode *ip);

// virtio_disk.c
void virtio_disk_init();
void virtio_disk_rw(struct buf *buffer, int write);
void virtio_disk_intr();

#endif
