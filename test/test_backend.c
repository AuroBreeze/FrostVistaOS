#include "user.h"
#include "libtest.h"

#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_CREAT 0x040

void _start(void)
{
	TEST_START("backend");

	int fd = open("/newfile", O_WRONLY | O_CREAT);
	TEST_ASSERT(fd < 0, "backend",
		    "ext4: O_CREAT should fail on read-only fs");

	fd = open("/newfile", O_RDONLY);
	TEST_ASSERT(fd < 0, "backend", "ext4: read missing file should fail");

	TEST_PASS("backend");
	shutdown();
}
