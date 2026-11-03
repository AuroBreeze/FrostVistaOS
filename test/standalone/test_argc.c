#include "user/user.h"
#include "user/libtest.h"

static volatile uint64 initialized_global = 123;
static volatile uint8 zero_global[32];

static void check_user_globals(void)
{
	TEST_ASSERT(initialized_global == 123, "argc",
		    "initialized global should retain its value");
	for (int i = 0; i < sizeof(zero_global); i++) {
		TEST_ASSERT(zero_global[i] == 0, "argc",
			    "BSS global should initially be zero");
		zero_global[i] = i + 1;
	}
	initialized_global = 456;
	TEST_ASSERT(initialized_global == 456 && zero_global[31] == 32, "argc",
		    "global variables should support reads and writes");
}

// The architecture entry preserves the argc/argv supplied by exec.
int main(int argc, char **argv)
{
	TEST_START("argc");
	check_user_globals();
	printf("Hello FrostVista OS!\n");
	printf("Let's check the arguments passed by exec:\n");

	printf("argc: %d\n", argc);

	for (int i = 0; i < argc; i++) {
		printf("argv[%d]: %s\n", i, argv[i]);
		// Print the strings you successfully pushed onto the user stack
		// via copyout!
		printf(argv[i]);
		printf("\n");
	}

	printf("Init process is exiting cleanly...\n");
	TEST_PASS("argc");
	shutdown();
	exit(0);
}
