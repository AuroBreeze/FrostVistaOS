
#include <stdint.h>
#include "uart.h"

void main(void){
    uart_init();
    uart_puts("Hello, FrostVista OS!\n");
    while(1){
    }
}


