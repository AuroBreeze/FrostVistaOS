// init.c

#define SYS_WRITE 1
#define SYS_EXIT 3

// Inline assembly to trigger a system call via 'ecall'
static inline long syscall(long num, long a0, long a1, long a2) {
  register long a0_asm __asm__("a0") = a0;
  register long a1_asm __asm__("a1") = a1;
  register long a2_asm __asm__("a2") = a2;
  register long a7_asm __asm__("a7") = num;

  __asm__ volatile("ecall"
                   : "+r"(a0_asm)
                   : "r"(a1_asm), "r"(a2_asm), "r"(a7_asm)
                   : "memory");

  return a0_asm;
}

// Wrapper for sys_write
long write(int fd, const char *buf, unsigned long count) {
  return syscall(SYS_WRITE, (long)fd, (long)buf, (long)count);
}

// Wrapper for sys_exit
void exit(int status) {
  syscall(SYS_EXIT, (long)status, 0, 0);
  while (1)
    ; // Trap the CPU if exit fails
}

void print(const char *str) {
  unsigned long len = 0;
  while (str[len] != '\0') {
    len++;
  }
  write(1, str, len);
}

void print_int(int num) {
  char buf[16];
  int i = 0;
  if (num == 0) {
    print("0");
    return;
  }
  while (num > 0) {
    buf[i++] = (num % 10) + '0';
    num /= 10;
  }
  // Print the digits in reverse order (correct to left-to-right)
  while (i > 0) {
    i--;
    write(1, &buf[i], 1);
  }
}

// According to RISC-V calling conventions, the compiler will
// automatically fetch 'argc' from 'a0' and 'argv' from 'a1'.
void _start(int argc, char *argv[]) {
  print("Hello FrostVista OS!\n");
  print("Let's check the arguments passed by exec:\n");

  print("argc: ");
  print_int(argc);
  print("\n");

  for (int i = 0; i < argc; i++) {
    print("argv[");
    print_int(i);
    print("]: ");
    // Print the strings you successfully pushed onto the user stack via
    // copyout!
    print(argv[i]);
    print("\n");
  }

  print("Init process is exiting cleanly...\n");
  exit(0);
}
