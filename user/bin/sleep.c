#include "user.h"

static int parse_seconds(const char *s, unsigned int *seconds)
{
	unsigned int value = 0;

	if (*s == '\0')
		return -1;

	for (; *s != '\0'; s++) {
		if (*s < '0' || *s > '9')
			return -1;
		if (value > (unsigned int) -1 / 10 ||
		    (value == (unsigned int) -1 / 10 &&
		     (unsigned int) (*s - '0') > (unsigned int) -1 % 10))
			return -1;
		value = value * 10 + (unsigned int) (*s - '0');
	}

	*seconds = value;
	return 0;
}

void _start(int argc, char **argv)
{
	unsigned int seconds;

	if (argc != 2 || parse_seconds(argv[1], &seconds) < 0) {
		printf("usage: sleep SECONDS\n");
		exit(1);
	}

	sleep(seconds);
	exit(0);
}
