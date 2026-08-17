#include "user.h"
#include "libtest.h"

static volatile int handled;

static void signal_handler(int sig)
{
	if (sig == SIGUSR1)
		handled = 1;
}

static void test_signal_round_trip(void)
{
	struct sigaction action = {0};

	TEST_START("test_signal_round_trip");
	action.handler = (uint64) signal_handler;
	action.flags = SA_RESTORER;
	action.restorer = (uint64) __restore;

	TEST_ASSERT(rt_sigaction(SIGUSR1, &action, 0) == 0,
		    "test_signal_round_trip", "sigaction should succeed");
	TEST_ASSERT(kill(getpid(), SIGUSR1) == 0, "test_signal_round_trip",
		    "kill should succeed");
	TEST_ASSERT(handled == 1, "test_signal_round_trip",
		    "handler should run before returning to user mode");
	TEST_PASS("test_signal_round_trip");
}

void _start(void)
{
	TEST_START("signal");
	test_signal_round_trip();
	TEST_PASS("signal");
	shutdown();
}
