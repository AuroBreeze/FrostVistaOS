#include "asm/riscv.h"
#include "asm/trap.h"
#include "driver/hal_console.h"
#include "kernel/defs.h"
#include "kernel/spinlock.h"
#include "kernel/types.h"
#include <stdarg.h>

static const char digits[] = "0123456789abcdef";
struct spinlock console_lock = {.name = "console_lock", .locked = 0, .cpu = 0};

#define RADIX_DEC 10
#define RADIX_HEX 16

void kputc(char c)
{
	hal_console_putc(c);
}

void kputs(const char *s)
{
	while (*s) {
		kputc(*s++);
	}
}

static void kprintint(long long xx, int base, int sign)
{
	char buf[32];
	int i = 0;
	uint64 x;

	if (sign && xx < 0) {
		x = -xx;
	} else {
		x = xx;
	}

	if (x == 0) {
		buf[i++] = '0';
	} else {
		while (x != 0) {
			buf[i++] = digits[x % base];
			x /= base;
		}
	}

	if (sign && xx < 0)
		buf[i++] = '-';

	while (--i >= 0)
		kputc(buf[i]);
}

static void kprintptr(uint64 x)
{
	kputs("0x");
	for (int i = 0; i < 16; i++, x <<= 4) {
		kputc(digits[(x >> 60) & 0xf]);
	}
}

void vkprintf(const char *fmt, va_list ap)
{
	for (int i = 0; fmt[i] != '\0'; i++) {
		if (fmt[i] != '%') {
			kputc(fmt[i]);
			continue;
		}

		char c = fmt[++i];
		if (c == '\0')
			break;

		switch (c) {
		case 'd':
			kprintint(va_arg(ap, int), RADIX_DEC, 1);
			break;
		case 'x':
			kprintint(va_arg(ap, uint), RADIX_HEX, 0);
			break;
		case 'p':
			kprintptr(va_arg(ap, uint64));
			break;
		case 's': {
			const char *s = va_arg(ap, const char *);
			if (!s)
				s = "(null)";
			kputs(s);
			break;
		}
		case 'c': {
			int ch = va_arg(ap, int);
			kputc((char) ch);
			break;
		}
		case '%':
			kputc('%');
			break;
		default:
			kputc('%');
			kputc(c);
			break;
		}
	}
}

void kprintf(const char *fmt, ...)
{
	if (!fmt) {
		kputs("kprintf: NULL fmt\n");
		while (1) {
		}
	}

	va_list ap;
	va_start(ap, fmt);
	acquire(&console_lock);
	vkprintf(fmt, ap);
	release(&console_lock);
	va_end(ap);
}

const char *log_ts(void)
{
	static char buf[12];
	uint64 t = r_time();
	int sec = (int)(t / 10000000);
	int ms = (int)((t % 10000000) / 10000);

	buf[0] = '[';
	if (sec < 10) {
		buf[1] = ' '; buf[2] = ' '; buf[3] = ' ';
		buf[4] = '0' + sec;
	} else if (sec < 100) {
		buf[1] = ' '; buf[2] = ' ';
		buf[3] = '0' + (sec / 10);
		buf[4] = '0' + (sec % 10);
	} else if (sec < 1000) {
		buf[1] = ' ';
		buf[2] = '0' + (sec / 100);
		buf[3] = '0' + ((sec / 10) % 10);
		buf[4] = '0' + (sec % 10);
	} else {
		buf[1] = '0' + ((sec / 1000) % 10);
		buf[2] = '0' + ((sec / 100) % 10);
		buf[3] = '0' + ((sec / 10) % 10);
		buf[4] = '0' + (sec % 10);
	}
	buf[5] = '.';
	buf[6] = '0' + ((ms / 100) % 10);
	buf[7] = '0' + ((ms / 10) % 10);
	buf[8] = '0' + (ms % 10);
	buf[9] = ']';
	buf[10] = ' ';
	buf[11] = '\0';
	return buf;
}

void _panic(const char *file, int line, const char *fmt, ...)
{
	intr_off();
	console_lock.locked = 0;

	kputs("\033[1;31m[KERNEL PANIC] at ");
	kputs(file);
	kputs(":");

	va_list ap;

	char line_fmt[] = "%d\nReason: ";
	va_start(ap, fmt);
	// kputs(file);
	// kputs(":");
	kprintint(line, 10, 0);
	kputs("\nReason: ");
	va_end(ap);

	va_start(ap, fmt);
	vkprintf(fmt, ap);
	va_end(ap);

	kputs("\033[0m\n");

	while (1) {
		__asm__ volatile("wfi");
	}
}
