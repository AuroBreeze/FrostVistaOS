#define UART_ADDR 0x10000000
#define UART_DATA_REG 0

void putchar(char ch) {
  *(volatile char *)(UART_ADDR + UART_DATA_REG) = ch;
}

__attribute__((noreturn)) void main() {
  char *str = "Hello World!\n";
  while (*str) {
    putchar(*str++);
  }
  while (1) {}
}