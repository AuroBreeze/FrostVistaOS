#include "user.h"
#include "libtest.h"

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20

#define PGSIZE 4096

static char file_data[PGSIZE + 8];

static void create_exit_mmap_file(void)
{
	for (int i = 0; i < PGSIZE + 8; i++)
		file_data[i] = 'x';
	file_data[0] = 'E';
	file_data[PGSIZE] = 'F';
	file_data[PGSIZE + 7] = 'Z';

	int fd = open("/mexit", O_WRONLY | O_CREAT | O_TRUNC);
	TEST_ASSERT(fd >= 0, "create_exit_mmap_file",
		    "should create exit mmap backing file");
	long n = write(fd, file_data, sizeof(file_data));
	TEST_ASSERT(n == sizeof(file_data), "create_exit_mmap_file",
		    "should write exit mmap backing file contents");
	TEST_ASSERT(close(fd) == 0, "create_exit_mmap_file",
		    "should close exit mmap backing file after writing");
}

static void child_maps_and_exits(void)
{
	char *anon = mmap(0, PGSIZE * 4, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(anon != (void *) -1, "mmap_exit_child",
		    "child anonymous mmap should succeed before exit");
	anon[0] = 'A';
	anon[PGSIZE * 3] = 'L';

	int fd = open("/mexit", O_RDONLY);
	TEST_ASSERT(fd >= 0, "mmap_exit_child",
		    "child should open file backing before exit");
	char *file = mmap(0, PGSIZE * 2, PROT_READ, MAP_PRIVATE, fd, 0);
	TEST_ASSERT(file != (void *) -1, "mmap_exit_child",
		    "child file-backed mmap should succeed before exit");
	TEST_ASSERT(close(fd) == 0, "mmap_exit_child",
		    "child should close fd while VMA keeps file alive");
	TEST_ASSERT(
	    file[0] == 'E' && file[PGSIZE] == 'F' && file[PGSIZE + 7] == 'Z',
	    "mmap_exit_child",
	    "child should fault anonymous and file-backed pages before exit");

	exit(0);
}

static void test_child_exit_releases_mmap_regions(void)
{
	TEST_START("test_child_exit_releases_mmap_regions");

	int pid = fork();
	TEST_ASSERT(pid >= 0, "test_child_exit_releases_mmap_regions",
		    "fork should succeed for mmap exit child");

	if (pid == 0)
		child_maps_and_exits();

	int status = -1;
	int reaped = waitpid(pid, &status, 0);
	TEST_ASSERT(reaped == pid, "test_child_exit_releases_mmap_regions",
		    "parent should reap mmap exit child");
	TEST_ASSERT(status == 0, "test_child_exit_releases_mmap_regions",
		    "mmap exit child should exit cleanly");

	char *anon = mmap(0, PGSIZE, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	TEST_ASSERT(anon != (void *) -1,
		    "test_child_exit_releases_mmap_regions",
		    "parent should still mmap after child exit cleanup");
	anon[0] = 'P';
	TEST_ASSERT(anon[0] == 'P', "test_child_exit_releases_mmap_regions",
		    "parent mapping after child exit should be usable");
	TEST_ASSERT(munmap(anon, PGSIZE) == 0,
		    "test_child_exit_releases_mmap_regions",
		    "parent should unmap post-exit anonymous mapping");

	int fd = open("/mexit", O_RDONLY);
	TEST_ASSERT(fd >= 0, "test_child_exit_releases_mmap_regions",
		    "parent should reopen file after child exit cleanup");
	char *file = mmap(0, PGSIZE * 2, PROT_READ, MAP_PRIVATE, fd, 0);
	TEST_ASSERT(file != (void *) -1,
		    "test_child_exit_releases_mmap_regions",
		    "parent should file-map after child exit cleanup");
	TEST_ASSERT(close(fd) == 0, "test_child_exit_releases_mmap_regions",
		    "parent should close fd after post-exit file mmap");
	TEST_ASSERT(file[0] == 'E' && file[PGSIZE] == 'F',
		    "test_child_exit_releases_mmap_regions",
		    "post-exit file mapping should read backing file contents");
	TEST_ASSERT(munmap(file, PGSIZE * 2) == 0,
		    "test_child_exit_releases_mmap_regions",
		    "parent should unmap post-exit file mapping");

	TEST_PASS("test_child_exit_releases_mmap_regions");
}

void _start(void)
{
	TEST_START("mmap_exit");
	create_exit_mmap_file();
	test_child_exit_releases_mmap_regions();
	TEST_PASS("mmap_exit");
	shutdown();
}
