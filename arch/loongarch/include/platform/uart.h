#ifndef __LOONGARCH_UART_H
#define __LOONGARCH_UART_H

#define DMW_MMIO_BASE 0x9000000000000000UL
#define UART_BASE (DMW_MMIO_BASE + 0x1fe001e0UL)
#define UART_THR (*(volatile unsigned char *) (UART_BASE + 0))
#define UART_LSR (*(volatile unsigned char *) (UART_BASE + 5))
#define UART_LSR_THRE 0x20

void uart_putc(char c);
void uart_puts(const char *s);

#endif
