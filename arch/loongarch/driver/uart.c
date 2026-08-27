#include "platform/uart.h"
#include "asm/cpu.h"
#include "asm/boot.h"
#include "kernel/types.h"
#include "platform/timer.h"
#include <stdarg.h>

static uint64 uart_base = UART_DMW1_BASE;

#define BootReg(reg) ((volatile unsigned char *) (UART_DMW1_BASE + (reg)))
#define BootReadReg(reg) (*(BootReg(reg)))
#define BootWriteReg(reg, data) (*(BootReg(reg)) = (data))

#define Reg(reg) ((volatile unsigned char *) (uart_base + (reg)))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, data) (*(Reg(reg)) = (data))

/* Minimal console used before the final kernel address space is active. */
BOOT_TEXT void boot_uart_init(void)
{
	BootWriteReg(LCR_adr, LCR_BAUD_LATCH);

	BootWriteReg(THR_adr, 0x03);
	BootWriteReg(IER_adr, 0x00);

	BootWriteReg(LCR_adr, LCR_WIDTH_C);
	BootWriteReg(FCR_adr, FCR_FIFO_ENABLE | FCR_RX_CLEAR | FCR_TX_CLEAR);
}

BOOT_TEXT void boot_uart_putc(char c)
{
	while ((BootReadReg(LSR_adr) & LSR_TX_IDLE) == 0)
		;

	BootWriteReg(THR_adr, c);
}

BOOT_TEXT void boot_uart_puts(const char *s)
{
	if (s == 0)
		return;

	while (*s)
		boot_uart_putc(*s++);
}

void uart_init()
{

	WriteReg(LCR_adr, LCR_BAUD_LATCH);

	WriteReg(0, 0x03);
	WriteReg(1, 0x00);

	WriteReg(LCR_adr, LCR_WIDTH_C);
	// WriteReg(IER_adr, IER_RX_ENABLE | IER_TX_ENABLE);
	WriteReg(FCR_adr, FCR_FIFO_ENABLE | FCR_RX_CLEAR | FCR_TX_CLEAR);
}

void uart_use_mapped_io(void)
{
	uart_base = UART_HIGH_BASE;
}

void uart_putc(char c)
{
	while ((ReadReg(LSR_adr) & LSR_TX_IDLE) == 0)
		;
	WriteReg(THR_adr, c);
}

void uart_puts(const char *s)
{
	while (*s) {
		uart_putc(*s++);
	}
}

int uart_getc()
{
	while ((ReadReg(LSR_adr) & LSR_RX_READY) == 0)
		return -1;
	return ReadReg(RHR_adr);
}

void hal_console_putc(char c)
{
	uart_putc(c);
}

void hal_console_puts(const char *s)
{
	while (*s) {
		hal_console_putc(*s++);
	}
}

int hal_console_getc(void)
{
	return uart_getc();
}

static const char digits[] = "0123456789abcdef";

#define RADIX_DEC 10
#define RADIX_HEX 16

void kputc(char c)
{
	uart_putc(c);
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
	vkprintf(fmt, ap);
	va_end(ap);
}

const char *log_ts(void)
{
	static char buf[12];
	uint64 t = r_time();
	int sec = (int) (t / 10000000);
	int ms = (int) ((t % 10000000) / 10000);

	buf[0] = '[';
	if (sec < 10) {
		buf[1] = ' ';
		buf[2] = ' ';
		buf[3] = ' ';
		buf[4] = '0' + sec;
	} else if (sec < 100) {
		buf[1] = ' ';
		buf[2] = ' ';
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
		cpu_wait();
	}
}
