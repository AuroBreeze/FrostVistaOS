#include "platform/uart.h"

void uart_putc(char c)
{
	while ((UART_LSR & UART_LSR_THRE) == 0)
		;
	UART_THR = (unsigned char) c;
}

void uart_puts(const char *s)
{
	while (*s != '\0')
		uart_putc(*s++);
}
