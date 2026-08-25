// include/driver/hal_console.h
#ifndef __DRIVER_CONSOLE_H
#define __DRIVER_CONSOLE_H

#include "platform/uart.h"

static inline void arch_console_putc(char c)
{
	hal_console_putc(c);
}

static inline int arch_console_getc(void)
{
	return hal_console_getc();
}

static inline void arch_console_puts(const char *s)
{
	while (*s) {
		arch_console_putc(*s++);
	}
}

#endif
