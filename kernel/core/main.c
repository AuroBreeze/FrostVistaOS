#include "driver/uart.h"

void display_banner(void) {
  uart_puts("    ______                __ _    ___      __       \n");
  uart_puts("   / ____/________  _____/ /| |  / (_)____/ /_____ _\n");
  uart_puts("  / /_  / ___/ __ \\/ ___/ __/ | / / / ___/ __/ __ `/\n");
  uart_puts(" / __/ / /  / /_/ (__  ) /_ | |/ / (__  ) /_/ /_/ / \n");
  uart_puts("/_/   /_/   \\____/____/\\__/ |___/_/____/\\__/\\__,_/  \n");
}

void main(void) {
  uart_init();
  display_banner();
  uart_puts("Hello, FrostVista OS!\n");
  kprintf("FrostVistaOS booting...\n");
  kprintf("uart_init ok, x=%d, ptr=%p\n", 42, main);
  while (1) {
  }
}
