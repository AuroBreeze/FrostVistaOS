#ifndef DEFS_H
#define DEFS_H

#include "kernel/types.h"
struct spinlock;

// proc.c
int cpuid();
struct cpu *get_cpu();
struct Process *get_proc();
void push_off();

// spinlock.c
void initlock(struct spinlock *lk, char *name);
int holding(struct spinlock *lk);
void push_off(void);
void pop_off(void);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
void sleep(void *chan, struct spinlock *lk);
void wakeup(void *chan);

// kalloc.c
void kalloc_init();
void kfree(void *pa);
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
void syscall();

// exec.c
int exec();

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

// vfs.c
struct vfs_node;

void vfs_init();
struct vfs_node *vfs_lookup(struct vfs_node *node, char *path);
void test_vfs();

#endif
