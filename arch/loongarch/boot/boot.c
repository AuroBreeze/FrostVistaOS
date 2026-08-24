#define DMW_MMIO_BASE 0x9000000000000000UL
#define UART_BASE (DMW_MMIO_BASE + 0x1fe001e0UL)
#define UART_THR (*(volatile unsigned char *) (UART_BASE + 0))
#define UART_LSR (*(volatile unsigned char *) (UART_BASE + 5))
#define UART_LSR_THRE 0x20

static void uart_putc(char c)
{
	while ((UART_LSR & UART_LSR_THRE) == 0)
		;
	UART_THR = (unsigned char) c;
}

static void uart_puts(const char *s)
{
	while (*s != '\0')
		uart_putc(*s++);
}

void loong_early_boot(void)
{
	uart_puts("\nFrostVista LoongArch kernel started\n");
	for (;;)
		;
}
