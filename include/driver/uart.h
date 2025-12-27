#ifndef UART_H
#define UART_H

#include "kernel/types.h"

#define UART_IRQ 0xa
// find device tree
#define UART0_BASE 0x10000000UL

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

// Read UART received Data
#define Reg(reg) ((volatile unsigned char *)(UART0_BASE + reg))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, data) (*(Reg(reg)) = data)

#define RXBUF_SIZE 128
static volatile char rxbuf[RXBUF_SIZE];
static volatile uint64 rx_w = 0;
static volatile uint64 rx_r = 0;

static inline int rx_empty(void) { return rx_r == rx_w; }
static inline int rx_full(void) { return (rx_w - rx_r) >= RXBUF_SIZE; }

#define TXBUF_SIZE 128
static volatile char txbuf[TXBUF_SIZE];
static volatile uint64 tx_w = 0;
static volatile uint64 tx_r = 0;

static inline int tx_empty(void) { return tx_r == tx_w; }
static inline int tx_full(void) { return (tx_w - tx_r) >= TXBUF_SIZE; }

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
static void tx_put(char c) {
  while (tx_full())
    ;
  txbuf[tx_w % TXBUF_SIZE] = c;
  tx_w++;
}

static int tx_get(void) {
  if (tx_empty())
    return -1;
  char c = txbuf[tx_r % TXBUF_SIZE];
  tx_r++;
  return (unsigned char)c;
}

static inline void uart_txintr_on(void) {
  uint8 ier = ReadReg(IER_adr);
  WriteReg(IER_adr, ier | IER_TX_ENABLE);
}

static inline void uart_txintr_off(void) {
  uint8 ier = ReadReg(IER_adr);
  WriteReg(IER_adr, ier & ~IER_TX_ENABLE);
}

#endif
