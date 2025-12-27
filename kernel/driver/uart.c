#include "driver/uart.h"
#include "driver/clint.h"
#include "kernel/riscv.h"

static inline int tx_ready() { return (ReadReg(LSR_adr) & LSR_TX_IDLE) != 0; }

static void uart_putintr() {
  while (tx_ready()) {
    int c = tx_get();
    if (c < 0)
      break;
    WriteReg(THR_adr, (char)c);
  }

  if (tx_empty()) {
    uart_txintr_off(); // Disbale interrupts promptly to prevent interrupt
    // storms
  } else {
    uart_txintr_on(); // There is still data to entered. Keep iit enabled.
  }
}

void uartintr() {
  // RX: Read all data that clear interrupt
  while (ReadReg(LSR_adr) & LSR_RX_READY) {
    char c = (char)ReadReg(RHR_adr);
    rx_put(c);
  }

  uart_putintr();
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
  int was_empty = tx_empty();

  tx_put(c);

  if (was_empty) {
    // NOTE:
    // Data must be written once first
    uart_putintr();
  }
}

void uart_puts(const char *s) {
  while (*s) {
    uart_putc(*s++);
  }
}

int uart_getc() { return rx_get(); }
