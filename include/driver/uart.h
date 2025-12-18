#ifndef UART_H
#define UART_H

// find device tree
#define UART0_BASE 0x10000000UL

void uart_init();
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc();

#endif
