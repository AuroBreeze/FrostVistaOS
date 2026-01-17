#include "uart.h"

void uart_init(void){
    WriteReg(UART_FCR_ADR, UART_FCR_FIFO_ENABLE | UART_FCR_CLEAR_RX | UART_FCR_CLEAR_TX);
    WriteReg(UART_LCR_ADR, UART_LCR_WORD_LENGTH);
    WriteReg(UART_LCR_ADR, UART_LCR_DIVISOR_LATCH_ENABLE);

    WriteReg(UART_DLH_ADR, 0x00);
    WriteReg(UART_DLL_ADR, 0x03);

    WriteReg(UART_LCR_ADR, (0 << 7));
    WriteReg(UART_FCR_ADR, UART_FCR_FIFO_LENGTH_ENABLE);
}

void uart_putc(char c){
    while((ReadReg(UART_LSR_ADR) & UART_LSR_TX_EMPTY) == 0);
    WriteReg(UART_TX_ADR, c);
}

