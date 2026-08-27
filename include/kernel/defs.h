#ifndef DEFS_H
#define DEFS_H

#include "kernel/fs.h"
#include "kernel/types.h"

struct spinlock;
struct buf;
struct file;
struct pipe;
struct Process;

void timerintr(void);
void timer_init(void);
int kmalloc_cache_init();

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
int refcnt_inc(uint64 va);
int refcnt_dec(uint64 va);
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

// syscall.c
int fetch_user_str(pagetable_t pagetable, char *dst, uint64 src_va,
		   uint64 max_len);
void argint(int n, int *ip);
void argaddr(int n, uint64 *ip);
int argstr(int n, char *buf, int max);
void syscall();

// tty.c
void terminal_claim_input(int pid);
uint64 sys_terminal_claim_input(void);
void terminal_release_input(void);
void terminal_forget_process(struct Process *p);

// exec.c
int exec(char *path);
int execve_kernel(char *path, char argv[][128], int argc);

// signal.c
void signal_reset_on_exec(struct Process *p);
void signal_send(struct Process *p, int sig);
int signal_pending(struct Process *p);
void check_signal(struct Process *proc);

// proc.c
int cpuid();
struct cpu *get_cpu();
struct Process *get_proc();
int get_pid(void);
void user_init();
void procinit(void);
void scheduler(void);
void sched(void);
void yield(void);
int alloc_fd(struct Process *p, struct file *f);
int fd_alloc();
int fork();
int exit(int exit_code);
uint64 wait4(int pid, uint64 wstatus, int options);
uint64 brk(uint64 addr);
int kill(int pid, int sig);

// mmap.c
struct vm_area_struct *find_overlapping_vma(uint64 addr, uint64 len);
uint64 do_mmap(uint64 addr, uint64 len, int prot, int flags, int fd,
	       uint64 offset);
int do_munmap(uint64 addr, uint64 len);

// pipe.c
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
int dup_from(int fd, int minfd);
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
void vfs_make_absolute_path(char *dst, const char *path);
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
struct vfs_inode *get_inode(uint32 dev, uint32 ino, int alloc);
void put_inode(struct vfs_inode *ip, int free);

// virtio_disk.c
void virtio_disk_init();
void virtio_disk_rw(struct buf *buffer, int write);
void virtio_disk_intr();

#endif
