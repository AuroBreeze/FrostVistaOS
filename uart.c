#include <stdint.h>
#include "uart.h"

#define UART0_BASE 0x10000000UL

void uart_putc(char c){
    volatile uint8_t *uart = (uint8_t *)UART0_BASE;
    *uart = (uint8_t)c;
}


void uart_puts(const char *s){
    while(*s){
        uart_putc(*s++);
    }
}
