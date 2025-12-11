#include "driver/uart.h"

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
#define LCR_BAUD_LATCH (1<<7)

// IS the receiving end ready?
#define LSR_RX_READY (1 << 0)
// IS the sender idle?
#define LSR_TX_IDLE (1 << 5)

// Read UART received Data
#define Reg(reg) ((volatile unsigned char *)(UART0_BASE + reg))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, data) (*(Reg(reg)) = data)


void uart_init(){

    WriteReg(LCR_adr, LCR_BAUD_LATCH);

    WriteReg(0, 0x03);
    WriteReg(1, 0x00);

    WriteReg(LCR_adr, LCR_WIDTH_C);
    // WriteReg(IER_adr, IER_RX_ENABLE | IER_TX_ENABLE);
    WriteReg(FCR_adr, FCR_FIFO_ENABLE | FCR_RX_CLEAR | FCR_TX_CLEAR);
}

void uart_putc(char c){
    while((ReadReg(LSR_adr) & LSR_TX_IDLE) == 0) ;
    WriteReg(THR_adr, c);
}

void uart_puts(const char *s){
    while(*s){
        uart_putc(*s++);
    }
}

int uart_getc(){
    while((ReadReg(LSR_adr) & LSR_RX_READY) == 0) 
        return -1;
    return ReadReg(RHR_adr);
}
