#include "platform/uart.h"
#include "asm/boot.h"
#include "kernel/types.h"
#include "asm/irq.h"
#include "kernel/irq.h"
#include "platform/eiointc.h"
#include "platform/pch_pic.h"

static uint64 uart_base = UART_DMW1_BASE;

#define RXBUF_SIZE 128U
#define TXBUF_SIZE 128U

static char rxbuf[RXBUF_SIZE];
static uint64 rx_w;
static uint64 rx_r;
static char txbuf[TXBUF_SIZE];
static uint64 tx_w;
static uint64 tx_r;

static int uart_irq_enabled;

/* Buffer access runs on CPU 0 with local interrupts disabled. */
static inline int rx_empty(void)
{
	return rx_r == rx_w;
}

static inline int rx_full(void)
{
	return rx_w - rx_r >= RXBUF_SIZE;
}

static void rx_put(char c)
{
	if (!rx_full())
		rxbuf[rx_w++ % RXBUF_SIZE] = c;
}

static int rx_get(void)
{
	if (rx_empty())
		return -1;
	return (unsigned char) rxbuf[rx_r++ % RXBUF_SIZE];
}

static inline int tx_empty(void)
{
	return tx_r == tx_w;
}

static inline int tx_full(void)
{
	return tx_w - tx_r >= TXBUF_SIZE;
}

static void tx_put(char c)
{
	txbuf[tx_w++ % TXBUF_SIZE] = c;
}

static int tx_get(void)
{
	if (tx_empty())
		return -1;
	return (unsigned char) txbuf[tx_r++ % TXBUF_SIZE];
}

static inline int tx_ready(void)
{
	return (ReadReg(LSR_adr) & LSR_TX_IDLE) != 0;
}

static inline void uart_txintr_on(void)
{
	WriteReg(IER_adr, ReadReg(IER_adr) | IER_TX_ENABLE);
}

static inline void uart_txintr_off(void)
{
	WriteReg(IER_adr, ReadReg(IER_adr) & ~IER_TX_ENABLE);
}

static void uart_putintr(void)
{
	while (tx_ready()) {
		int c = tx_get();
		if (c < 0)
			break;
		WriteReg(THR_adr, (char) c);
	}
	/* Disable empty-buffer TX interrupts to avoid an interrupt storm. */
	if (tx_empty())
		uart_txintr_off();
	else
		uart_txintr_on();
}

static void tty_handle_intr(char c)
{
	if (c == 0x03) {
		extern void terminal_release_input(void);
		terminal_release_input();
		return;
	}
	rx_put(c);
}

void uartintr(void)
{
	/* Drain RX before advancing TX, as on RISC-V. */
	while (ReadReg(LSR_adr) & LSR_RX_READY)
		tty_handle_intr((char) ReadReg(RHR_adr));
	uart_putintr();
}

static int uart_interrupt(void *arg)
{
	(void) arg;
	if (ReadReg(IIR_adr) & IIR_NO_INTERRUPT)
		return 0;
	uartintr();
	return 1;
}

/* Called after platform_irq_init() has initialized both controllers. */
int uart_irq_init(void)
{
	int saved = irq_save();
	if (uart_irq_enabled) {
		irq_restore(saved);
		return 0;
	}
	if (pch_pic_configure(UART_IRQ) < 0 ||
	    eiointc_configure_vector(UART_IRQ) < 0 ||
	    irq_register(UART_IRQ, uart_interrupt, 0) < 0) {
		irq_restore(saved);
		return -1;
	}
	uart_irq_enabled = 1;
	WriteReg(IER_adr, IER_RX_ENABLE);
	eiointc_enable_vector(UART_IRQ);
	pch_pic_unmask(UART_IRQ);
	irq_restore(saved);
	return 0;
}

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
	/* Keep receive interrupts masked until the route is installed. */
	WriteReg(IER_adr, 0);
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
	int saved = irq_save();
	int c =
	    uart_irq_enabled
		? rx_get()
		: ((ReadReg(LSR_adr) & LSR_RX_READY) ? ReadReg(RHR_adr) : -1);
	irq_restore(saved);
	return c;
}

void hal_console_putc(char c)
{
	int saved = irq_save();
	if (!uart_irq_enabled) {
		/* Early boot has no active UART interrupt route. */
		uart_putc(c);
		irq_restore(saved);
		return;
	}
	/* A full queue must progress even when the caller holds a spinlock. */
	while (tx_full())
		uart_putintr();
	int was_empty = tx_empty();
	tx_put(c);
	if (was_empty)
		uart_putintr();
	/* Panic and locked-console output cannot wait for a later interrupt. */
	if (!saved)
		while (!tx_empty())
			uart_putintr();
	irq_restore(saved);
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
