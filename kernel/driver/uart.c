#include "driver/uart.h"
#include "driver/clint.h"
#include "kernel/riscv.h"

// TODO:
// Implementing TX interrupt handling
static void rx_put(char c) {
  if (rx_full()) {
    return;
  }
  rxbuf[rx_w % RXBUF_SIZE] = c;
  rx_w++;
}

static int rx_get(void) {
  if (rx_empty())
    return -1;
  char c = rxbuf[rx_r % RXBUF_SIZE];
  rx_r++;
  return (unsigned char)c;
}

void uartintr() {
  // RX: Read all data that clear interrupt
  while (ReadReg(LSR_adr) & LSR_RX_READY) {
    char c = (char)ReadReg(RHR_adr);
    rx_put(c);
  }
}

void pre_uart_init() {
  w_sie(r_sie() | SIE_SEIE);
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

void uart_init() {

  WriteReg(LCR_adr, LCR_BAUD_LATCH);

  WriteReg(0, 0x03);
  WriteReg(1, 0x00);

  WriteReg(LCR_adr, LCR_WIDTH_C);
  WriteReg(FCR_adr, FCR_FIFO_ENABLE | FCR_RX_CLEAR | FCR_TX_CLEAR);
  // Interrupt Enable Register: enable RX and TX interrupt

  WriteReg(IER_adr, IER_RX_ENABLE);
}

void uart_putc(char c) {
  while ((ReadReg(LSR_adr) & LSR_TX_IDLE) == 0)
    ;
  WriteReg(THR_adr, c);
}

void uart_puts(const char *s) {
  while (*s) {
    uart_putc(*s++);
  }
}

int uart_getc() { return rx_get(); }
