#include "user.h"

void _start(int argc, char *argv[])
{
	write(1, "\033[2J\033[H", 7);
	exit(0);
}
