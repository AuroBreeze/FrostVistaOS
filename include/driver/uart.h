#ifndef UART_H
#define UART_H

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

#endif
