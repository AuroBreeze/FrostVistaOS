// include/driver/hal_console.h
#ifndef __HAL_CONSOLE_H
#define __HAL_CONSOLE_H

void hal_console_putc(char c);        // uart_putc
void hal_console_puts(const char *s); // uart_puts
int hal_console_getc(void);           // uart_getc

#endif
