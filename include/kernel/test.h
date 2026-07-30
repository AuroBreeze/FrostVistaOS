#ifndef __TEST_H__
#define __TEST_H__

#include "kernel/log.h"

#define TEST_ASSERT(cond, msg)                                                 \
	do {                                                                   \
		if (!(cond)) {                                                 \
			panic(msg);                                            \
		}                                                              \
	} while (0)

#define TEST_LOG(fmt, ...) LOG_INFO("  " fmt, ##__VA_ARGS__)
#define RUN_TEST(fn)                                                           \
	do {                                                                   \
		TEST_LOG("[RUN] %s", #fn);                                     \
		fn();                                                          \
		TEST_LOG("[OK] %s", #fn);                                      \
	} while (0)

#endif
