#include "user/user.h"
#include "user/libtest.h"

#define TEST_NAME "time"

static uint64 now_us(void)
{
	struct linux_timeval tv;
	TEST_ASSERT(gettimeofday(&tv, 0) == 0, TEST_NAME,
		    "gettimeofday should succeed");
	TEST_ASSERT(tv.tv_usec < 1000000, TEST_NAME,
		    "timeval microseconds should be normalized");
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

static void test_clock_units(void)
{
	struct linux_timespec ts;
	uint64 before = now_us();
	TEST_ASSERT(clock_gettime(0, &ts) == 0, TEST_NAME,
		    "clock_gettime should succeed");
	uint64 after = now_us();
	TEST_ASSERT(ts.tv_nsec < 1000000000 && ts.tv_nsec % 1000 == 0,
		    TEST_NAME, "timespec should have microsecond resolution");
	uint64 clock_us = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
	TEST_ASSERT(before <= clock_us && clock_us <= after, TEST_NAME,
		    "clock_gettime and gettimeofday should use the same units");

	before = now_us();
	long ticks = times(0);
	after = now_us();
	TEST_ASSERT(ticks >= 0 && (uint64) ticks >= before / 10000 &&
			(uint64) ticks <= after / 10000,
		    TEST_NAME, "times should return 100 ticks per second");
}

static void test_sleep(void)
{
	struct linux_timespec req = {.tv_sec = 1, .tv_nsec = 0};
	/* Let the host time this sleep independently of the guest clocks. */
	printf("TIME_SLEEP_BEGIN\n");
	uint64 before = now_us();
	TEST_ASSERT(nanosleep(&req) == 0, TEST_NAME,
		    "one-second nanosleep should succeed");
	uint64 after = now_us();
	printf("TIME_SLEEP_END\n");
	printf("TIME_SLEEP_ELAPSED_US=%d\n", (int) (after - before));
	TEST_ASSERT(after >= before && after - before >= 1000000, TEST_NAME,
		    "nanosleep must not return before the requested duration");

	req.tv_sec = 0;
	req.tv_nsec = 1;
	before = now_us();
	TEST_ASSERT(nanosleep(&req) == 0, TEST_NAME,
		    "sub-microsecond nanosleep should succeed");
	TEST_ASSERT(now_us() > before, TEST_NAME,
		    "positive nanosleep should not round down to zero");

	req.tv_nsec = 1000000000;
	TEST_ASSERT(nanosleep(&req) == -1, TEST_NAME,
		    "nanosleep should reject invalid nanoseconds");
	req.tv_nsec = 0;
	req.tv_sec = ~0ULL;
	TEST_ASSERT(nanosleep(&req) == -1, TEST_NAME,
		    "nanosleep should reject overflowing durations");
}

int main(int argc, char **argv)
{
	TEST_START(TEST_NAME);
	test_clock_units();
	test_sleep();
	TEST_PASS(TEST_NAME);
	shutdown();
}
