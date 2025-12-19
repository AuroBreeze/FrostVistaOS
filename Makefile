CROSS  = riscv64-elf
CC     = $(CROSS)-gcc
DUMP   = $(CROSS)-objdump


CFLAGS = -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany \
         -nostdlib -nostartfiles -ffreestanding -O2 -Iinclude


LDFLAGS = -T linker.ld

SRCDIRS = . kernel/core kernel/driver kernel/arch/riscv kernel/mm kernel/tool

C_SRCS := $(foreach d,$(SRCDIRS),$(wildcard $(d)/*.c))
S_SRCS := $(foreach d,$(SRCDIRS),$(wildcard $(d)/*.S))

OBJS   := $(C_SRCS:.c=.o) $(S_SRCS:.S=.o)

QEMU      = qemu-system-riscv64
QEMUFLAGS = -machine virt -nographic -bios none -kernel kernel.elf

.PHONY: all clean run

all: kernel.elf

disasm: kernel.elf
	$(DUMP) -dS kernel.elf > disasm.txt

kernel.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

run: kernel.elf
	$(QEMU) $(QEMUFLAGS)

clean:
	rm -f $(OBJS) kernel.elf disasm.txt

