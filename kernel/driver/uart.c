#include "driver/uart.h"
void uart_init() {

  WriteReg(LCR_adr, LCR_BAUD_LATCH);

  WriteReg(0, 0x03);
  WriteReg(1, 0x00);

  WriteReg(LCR_adr, LCR_WIDTH_C);
  // WriteReg(IER_adr, IER_RX_ENABLE | IER_TX_ENABLE);
  WriteReg(FCR_adr, FCR_FIFO_ENABLE | FCR_RX_CLEAR | FCR_TX_CLEAR);
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

int uart_getc() {
  while ((ReadReg(LSR_adr) & LSR_RX_READY) == 0)
    return -1;
  return ReadReg(RHR_adr);
}
