MAKEFLAGS += -j$(shell nproc)

# Set the default ARCH to riscv
ARCH ?= riscv

LOG_NUM ?= 2

ifeq ($(LOG), TRACE)
	LOG_NUM = 0
else ifeq ($(LOG), DEBUG)
	LOG_NUM = 1
else ifeq ($(LOG), INFO)
	LOG_NUM = 2
else ifeq ($(LOG), WARN)
	LOG_NUM = 3
else ifeq ($(LOG), ERROR)
	LOG_NUM = 4
endif

ifeq ($(ARCH), riscv)
	CROSS = riscv64-elf
	ARCH_CFLAGS = -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany
	LINKER_SCRIPT = arch/$(ARCH)/linker.ld
	QEMU = qemu-system-riscv64
	QEMUFLAGS = -machine virt -nographic -bios none -kernel kernel.elf
endif

CC     = $(CROSS)-gcc
DUMP   = $(CROSS)-objdump

# Set the default include path
INCLUDES = -Iinclude -Iarch/$(ARCH)/include

CFLAGS = $(ARCH_CFLAGS) -nostdlib -nostartfiles -ffreestanding -O2 $(INCLUDES)
CFLAGS += -DCURRENT_LOG_LEVEL=$(LOG_NUM)

LDFLAGS = -T $(LINKER_SCRIPT)

# Obtain all common kernel C files
KERNEL_C := $(wildcard kernel/*/*.c)

# Get all architecture specific C/S files
ARCH_C := $(wildcard arch/$(ARCH)/*/*.c)
ARCH_S := $(wildcard arch/$(ARCH)/*/*.S)

OBJS := $(KERNEL_C:.c=.o) $(ARCH_C:.c=.o) $(ARCH_S:.S=.o)

.PHONY: all clean run

all:
	$(MAKE) clean
	$(MAKE) -j$(nproc) kernel.elf
	$(MAKE) run

disasm: kernel.elf
	$(DUMP) -dS kernel.elf > disasm.txt

kernel.elf: $(OBJS) $(LINKER_SCRIPT)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

run: kernel.elf
	$(QEMU) $(QEMUFLAGS)

clean:
	rm -f $(OBJS) kernel.elf disasm.txt

