#ifndef __USER_H__
#define __USER_H__

#define SYS_write 1
#define SYS_fork 2
#define SYS_exit 3
#define SYS_wait 4
#define SYS_sbrk 5
#define SYS_open 6
#define SYS_read 7
#define SYS_close 8
#define SYS_dup 9

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

typedef long int64;

typedef uint64 *pagetable_t;
typedef uint64 pte_t;

// --- System Call Wrappers ---
long write(int fd, const char *buf, uint64 count);
void exit(int status) __attribute__((noreturn));
void *sbrk(int increment);
int fork(void);
int wait(void);
int open(const char *path, int flags);
long read(int fd, void *buf, uint64 count);
int close(int fd);
int dup(int fd);

// --- Simple Library Functions ---
void printf(const char *fmt, ...);
void print_int(int num);
unsigned long strlen(const char *s);

#endif
