#include <stddef.h>

#define DMW_RAM_BASE 0x8000000000000000UL
#define DMW_MMIO_BASE 0x9000000000000000UL
#define UART_BASE (DMW_MMIO_BASE + 0x1fe001e0UL)
#define UART_THR (*(volatile unsigned char *) (UART_BASE + 0))
#define UART_LSR (*(volatile unsigned char *) (UART_BASE + 5))
#define UART_LSR_THRE 0x20

#define KERNEL_VMA_BASE 0xffffffc080000000UL
#define KERNEL_LMA_BASE 0x0000000000000000UL

#define PT_LOAD 1

typedef unsigned long uint64;

struct elf64_header {
	unsigned char ident[16];
	unsigned short type;
	unsigned short machine;
	unsigned int version;
	uint64 entry;
	uint64 phoff;
	uint64 shoff;
	unsigned int flags;
	unsigned short ehsize;
	unsigned short phentsize;
	unsigned short phnum;
	unsigned short shentsize;
	unsigned short shnum;
	unsigned short shstrndx;
};

struct elf64_program_header {
	unsigned int type;
	unsigned int flags;
	uint64 offset;
	uint64 vaddr;
	uint64 paddr;
	uint64 filesz;
	uint64 memsz;
	uint64 align;
};

extern unsigned char _kernel_blob_start[];

static void uart_putc(char c)
{
	while ((UART_LSR & UART_LSR_THRE) == 0)
		;
	UART_THR = (unsigned char) c;
}

static void uart_puts(const char *s)
{
	while (*s != '\0')
		uart_putc(*s++);
}

static void byte_copy(unsigned char *dst, const unsigned char *src, uint64 n)
{
	while (n-- != 0)
		*dst++ = *src++;
}

static void byte_zero(unsigned char *dst, uint64 n)
{
	while (n-- != 0)
		*dst++ = 0;
}

static void load_kernel(void)
{
	const struct elf64_header *ehdr =
	    (const struct elf64_header *) _kernel_blob_start;
	const struct elf64_program_header *phdr =
	    (const struct elf64_program_header *) (_kernel_blob_start +
						   ehdr->phoff);

	for (unsigned short i = 0; i < ehdr->phnum; i++) {
		if (phdr[i].type != PT_LOAD)
			continue;

		const unsigned char *src = _kernel_blob_start + phdr[i].offset;
		unsigned char *dst =
		    (unsigned char *) (DMW_RAM_BASE + phdr[i].paddr);

		byte_copy(dst, src, phdr[i].filesz);
		byte_zero(dst + phdr[i].filesz, phdr[i].memsz - phdr[i].filesz);
	}
}

__attribute__((noreturn)) static void jump_to_kernel(uint64 entry)
{
	uint64 physical_entry = entry - KERNEL_VMA_BASE + KERNEL_LMA_BASE;

	asm volatile("jirl $zero, %0, 0\n" : : "r"(physical_entry) : "memory");

	__builtin_unreachable();
}

void __attribute__((noreturn)) bootloader_main(void)
{
	const struct elf64_header *ehdr =
	    (const struct elf64_header *) _kernel_blob_start;

	uart_puts("B\n");
	load_kernel();
	uart_puts("L\n");
	jump_to_kernel(ehdr->entry);
}
