#include <user/user.h>
#include <user/libtest.h>

#define TEST_NAME "framework"
#define GETPID_ITERATIONS 1000000ULL

static int test_now_us(uint64 *result)
{
	struct linux_timespec ts;

	if (clock_gettime(0, &ts) < 0)
		return -1;

	*result = (ts.tv_sec * 1000000ULL) + (ts.tv_nsec / 1000ULL);
	return 0;
}

static void test_getpid_perf(void)
{
	uint64 start_time;
	uint64 end_time;

	TEST_ASSERT(test_now_us(&start_time) == 0, TEST_NAME,
		    "start clock read failed");
	for (uint64 i = 0; i < GETPID_ITERATIONS; ++i) {
		getpid();
	}
	TEST_ASSERT(test_now_us(&end_time) == 0, TEST_NAME,
		    "end clock read failed");
	TEST_ASSERT(end_time > start_time, TEST_NAME,
		    "benchmark clock did not advance");

	uint64 duration = (end_time - start_time);
	uint64 avg_ns = duration * 1000ULL / GETPID_ITERATIONS;

	printf("getpid iterations: %lu\n", (unsigned long) GETPID_ITERATIONS);
	printf("Duration: %lu us\n", duration);
	printf("Avg: %lu ns/call (including loop overhead)\n", avg_ns);
}

void _start(void)
{
	TEST_START(TEST_NAME);
	test_getpid_perf();
	TEST_PASS(TEST_NAME);
	shutdown();
}
