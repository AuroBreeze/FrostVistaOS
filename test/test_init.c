#include "user.h"

// TODO: Implement a simple shell
int main()
{
	static char buf[4] = {0};

	while (1) {
		printf("$ ");
		read(0, buf, sizeof(buf) - 1);

		if (buf[0] == 0)
			continue;
		printf("%s\n", buf);
		printf("You typed: %s\n", buf);
	}
	exit(0);
}
