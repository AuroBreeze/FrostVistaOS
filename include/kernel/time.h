#ifndef FV_KERNEL_TIME_H
#define FV_KERNEL_TIME_H

#include "kernel/types.h"

struct timeval {
	uint64 tv_sec;
	uint64 tv_usec;
};

struct timespec {
	uint64 tv_sec;
	uint64 tv_nsec;
};

struct tms {
	uint64 tms_utime;
	uint64 tms_stime;
	uint64 tms_cutime;
	uint64 tms_cstime;
};

#endif
