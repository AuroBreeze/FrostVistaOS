#ifndef __FROSTVISTA_LOG_H__
#define __FROSTVISTA_LOG_H__

#define LOG_LEVEL_TRACE 0
#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_WARN 3
#define LOG_LEVEL_ERROR 4

#ifndef CURRENT_LOG_LEVEL
#define CURRENT_LOG_LEVEL LOG_LEVEL_INFO

#endif

#include "kernel/defs.h"

#ifndef LOG_MODULE
#define LOG_MODULE_TAG ""
#else
#define LOG_MODULE_TAG "[" LOG_MODULE "] "
#endif

#define LOG_COLOR_RED "\033[1;31m"
#define LOG_COLOR_GREEN "\033[1;32m"
#define LOG_COLOR_YELLOW "\033[1;33m"
#define LOG_COLOR_BLUE "\033[1;34m"
#define LOG_COLOR_CYAN "\033[1;36m"
#define LOG_COLOR_GRAY "\033[1;90m"
#define LOG_COLOR_DIM "\033[2m"
#define LOG_COLOR_RESET "\033[0m"

#define panic(fmt, ...) _panic(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_TRACE(fmt, ...)                                                    \
	do {                                                                   \
		if (CURRENT_LOG_LEVEL <= LOG_LEVEL_TRACE)                      \
			kprintf(LOG_COLOR_GRAY "%s[TRACE] " LOG_MODULE_TAG   \
				fmt LOG_COLOR_RESET "\n",                      \
				log_ts(), ##__VA_ARGS__);                      \
	} while (0)

#define LOG_INFO(fmt, ...)                                                     \
	do {                                                                   \
		if (CURRENT_LOG_LEVEL <= LOG_LEVEL_INFO)                       \
			kprintf(LOG_COLOR_GREEN "%s[ INFO] " LOG_MODULE_TAG  \
				fmt LOG_COLOR_RESET "\n",                      \
				log_ts(), ##__VA_ARGS__);                      \
	} while (0)

#define LOG_DEBUG(fmt, ...)                                                    \
	do {                                                                   \
		if (CURRENT_LOG_LEVEL <= LOG_LEVEL_DEBUG)                      \
			kprintf(LOG_COLOR_BLUE "%s[DEBUG] " LOG_MODULE_TAG   \
				fmt LOG_COLOR_RESET "\n",                      \
				log_ts(), ##__VA_ARGS__);                      \
	} while (0)

#define LOG_WARN(fmt, ...)                                                     \
	do {                                                                   \
		if (CURRENT_LOG_LEVEL <= LOG_LEVEL_WARN)                       \
			kprintf(LOG_COLOR_YELLOW "%s[ WARN] " LOG_MODULE_TAG \
				fmt LOG_COLOR_RESET "\n",                      \
				log_ts(), ##__VA_ARGS__);                      \
	} while (0)

#define LOG_ERROR(fmt, ...)                                                    \
	do {                                                                   \
		if (CURRENT_LOG_LEVEL <= LOG_LEVEL_ERROR)                      \
			kprintf(LOG_COLOR_RED                                  \
				"%s[ERROR] %s:%d: " LOG_MODULE_TAG             \
				fmt LOG_COLOR_RESET "\n",                      \
				log_ts(), __FILE__, __LINE__,                  \
				##__VA_ARGS__);                                \
	} while (0)

#define LOG_SEP()                                                              \
	kprintf(LOG_COLOR_DIM                                                    \
		"------------------------------------------------------------"  \
		LOG_COLOR_RESET "\n")

#define LOG_BANNER(fmt, ...)                                                   \
	kprintf(LOG_COLOR_CYAN fmt LOG_COLOR_RESET "\n", ##__VA_ARGS__)

#endif
