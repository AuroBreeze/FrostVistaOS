#include "user/user.h"

int main(int argc, char **argv)
{
	write(1, "\033[2J\033[H", 7);
	return 0;
}
