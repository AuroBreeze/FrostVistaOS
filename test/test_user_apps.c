#include "user.h"
#include "libtest.h"

#if defined(__loongarch__)
#define EXPECTED_ELF_MACHINE 258
#elif defined(__riscv)
#define EXPECTED_ELF_MACHINE 243
#else
#error Unsupported user application architecture
#endif

static void check_app(const char *path)
{
	unsigned char header[20];
	int fd = open(path, O_RDONLY);
	TEST_ASSERT(fd >= 0, "user_apps", "application exists in EasyFS");
	TEST_ASSERT(read(fd, header, sizeof(header)) == sizeof(header),
		    "user_apps", "read application ELF header");
	close(fd);
	TEST_ASSERT(header[0] == 0x7f && header[1] == 'E' && header[2] == 'L' &&
			header[3] == 'F' && header[4] == 2 && header[5] == 1,
		    "user_apps", "application is a little-endian ELF64 image");
	unsigned int machine = header[18] | ((unsigned int) header[19] << 8);
	TEST_ASSERT(machine == EXPECTED_ELF_MACHINE, "user_apps",
		    "application architecture matches the running kernel");
}

void _start(void)
{
	static const char *apps[] = {
	    "/init", "/cat",   "/clear", "/echo",  "/fvsh", "/ls", "/mkdir",
	    "/rm",   "/sleep", "/touch", "/uname", "/yes",  0,
	};
	TEST_START("user_apps");
	for (int i = 0; apps[i]; i++)
		check_app(apps[i]);

	int fd = open("/user-apps.out", O_WRONLY | O_CREAT | O_TRUNC);
	TEST_ASSERT(fd >= 0, "user_apps", "create application output file");
	int pid = fork();
	TEST_ASSERT(pid >= 0, "user_apps", "fork application process");
	if (pid == 0) {
		TEST_ASSERT(dup3(fd, STDOUT_FILENO, 0) >= 0, "user_apps",
			    "redirect application stdout");
		close(fd);
		char *argv[] = {"echo", "easyfs", "apps", 0};
		execv("/echo", argv);
		exit(127);
	}
	close(fd);
	int status = -1;
	TEST_ASSERT(waitpid(pid, &status, 0) == pid && status == 0, "user_apps",
		    "disk-backed echo executes and exits successfully");
	fd = open("/user-apps.out", O_RDONLY);
	TEST_ASSERT(fd >= 0, "user_apps", "open application output");
	char output[32] = {0};
	long count = read(fd, output, sizeof(output));
	close(fd);
	TEST_ASSERT(count == 12 && strncmp(output, "easyfs apps\n", 12) == 0,
		    "user_apps",
		    "application receives arguments and writes output");
	TEST_ASSERT(unlink("/user-apps.out") == 0, "user_apps",
		    "remove output");
	TEST_PASS("user_apps");
	shutdown();
}
