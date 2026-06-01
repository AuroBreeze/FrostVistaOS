#ifndef LIBTEST_H
#define LIBTEST_H

#define TEST_START(name) printf("=== TEST %s ===\n", name)
#define TEST_PASS(name)                                                        \
	do {                                                                   \
		printf("=== PASS: %s ===\n", name);                            \
	} while (0)
#define TEST_FAIL(name)                                                        \
	do {                                                                   \
		printf("=== FAIL: %s ===\n", name);                            \
		exit(1);                                                       \
	} while (0)
#define TEST_ASSERT(cond, name, msg)                                           \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: %s: %s\n", name, msg);                   \
			TEST_FAIL(name);                                       \
		}                                                              \
	} while (0)

#endif
