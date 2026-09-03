#ifndef __LOONGARCH_UART_H
#define __LOONGARCH_UART_H

#include "asm/machine.h"

#define UART_PHYS_BASE 0x1fe001e0ULL
#define UART_PAGE_PA (UART_PHYS_BASE & ~(0x1000ULL - 1ULL))
#define UART_PAGE_VA (KERNEL_IO_BASE + UART_PAGE_PA)
#define UART_HIGH_BASE (KERNEL_IO_BASE + UART_PHYS_BASE)
#define UART_DMW1_BASE (DMW1_BASE + UART_PHYS_BASE)

// recive the data
#define RHR_adr 0
#define THR_adr 0

// allows enabel or disable interrupt generetion by the uart
#define IER_adr 1

// FIFO Control Register
#define FCR_adr 2

// Line Control Register
#define LCR_adr 3
// Line Stauts Register
#define LSR_adr 5

// Enable receive and transmit interrupts
#define IER_RX_ENABLE (1 << 0)
#define IER_TX_ENABLE (1 << 1)

// Enable FIFO and clear tx and tx
#define FCR_FIFO_ENABLE (1 << 0)
#define FCR_RX_CLEAR (1 << 1)
#define FCR_TX_CLEAR (1 << 2)

// Set the character width to 8 bit
#define LCR_WIDTH_C (3 << 0)
// Enable modify Divider Latch
#define LCR_BAUD_LATCH (1 << 7)

// IS the receiving end ready?
#define LSR_RX_READY (1 << 0)
// IS the sender idle?
#define LSR_TX_IDLE (1 << 5)

void uart_init();
void uart_use_mapped_io(void);
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc(void);

void kprintf(const char *fmt, ...);
void _panic(const char *file, int line, const char *fmt, ...);
#define panic(fmt, ...) _panic(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

void hal_console_putc(char c);
void hal_console_puts(const char *s);
int hal_console_getc(void);

#endif
