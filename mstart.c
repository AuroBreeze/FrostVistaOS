extern void uart_putc(char c);
__attribute__((noreturn)) void main() {
  char *str = "Hello World!\n";
  while (*str) {
    uart_putc(*str++);
  }
  while (1) {}
}