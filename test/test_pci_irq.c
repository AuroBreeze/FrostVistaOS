#include "user.h"
#include "libtest.h"

#define BLOCK_SIZE 4096
#define BLOCK_COUNT 80

static unsigned char block[BLOCK_SIZE];

/* Exceed both the block cache and queue size, then verify disk readback. */
void _start(void)
{
	TEST_START("pci_irq");
	int fd = open("/irq-data", O_RDWR | O_CREAT | O_TRUNC);
	TEST_ASSERT(fd >= 0, "pci_irq", "create disk-backed file");
	for (int n = 0; n < BLOCK_COUNT; n++) {
		for (int i = 0; i < BLOCK_SIZE; i++)
			block[i] = (unsigned char) (n * 37 + i);
		TEST_ASSERT(write(fd, block, BLOCK_SIZE) == BLOCK_SIZE,
			    "pci_irq", "write completed through IRQ wakeup");
	}
	close(fd);
	fd = open("/irq-data", 0);
	TEST_ASSERT(fd >= 0, "pci_irq", "reopen disk-backed file");
	for (int n = 0; n < BLOCK_COUNT; n++) {
		memset(block, 0, BLOCK_SIZE);
		TEST_ASSERT(read(fd, block, BLOCK_SIZE) == BLOCK_SIZE,
			    "pci_irq", "read completed through IRQ wakeup");
		for (int i = 0; i < BLOCK_SIZE; i++)
			TEST_ASSERT(block[i] == (unsigned char) (n * 37 + i),
				    "pci_irq",
				    "disk data matches written pattern");
	}
	TEST_ASSERT(read(fd, block, 1) == 0, "pci_irq", "EOF returns zero");
	close(fd);
	TEST_PASS("pci_irq");
	shutdown();
}
