#include "platform/uart.h"
#include "asm/boot.h"
#include "kernel/types.h"

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
