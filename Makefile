CROSS  = riscv64-elf
CC     = $(CROSS)-gcc
CFLAGS = -march=rv64imac -mabi=lp64 -mcmodel=medany -nostdlib -nostartfiles -ffreestanding -O2
LDFLAGS= -T linker.ld

OBJS = start.o main.o uart.o

QEMU     = qemu-system-riscv64
QEMUFLAGS= -machine virt -nographic -bios none -kernel kernel.elf

.PHONY: all clean run

all: kernel.elf

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

run: kernel.elf
	$(QEMU) $(QEMUFLAGS)

clean:
	rm -f *.o kernel.elf


