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

#endif
