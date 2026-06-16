#ifndef _STAT_H__
#define _STAT_H__

#include "kernel/types.h"

#define T_DIR 1	   // Directory
#define T_FILE 2   // File
#define T_DEVICE 3 // Device

struct stat {
	int addr;    // Device ID (if device)
	short type;  // Type of file
	int nlink;   // Number of links to file
	uint64 size; // Size of file in bytes
};

// Linux RISC-V 64 stat layout returned to libc through fstat/newfstatat.
struct linux_stat {
	uint64 st_dev;
	uint64 st_ino;
	uint32 st_mode;
	uint32 st_nlink;
	uint32 st_uid;
	uint32 st_gid;
	uint64 st_rdev;
	uint64 __pad1;
	int64 st_size;
	int32 st_blksize;
	int32 __pad2;
	int64 st_blocks;
	int64 st_atime_sec;
	int64 st_atime_nsec;
	int64 st_mtime_sec;
	int64 st_mtime_nsec;
	int64 st_ctime_sec;
	int64 st_ctime_nsec;
	uint32 __unused[2];
};

#endif
