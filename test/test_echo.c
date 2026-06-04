#include "user.h"

void _start()
{
	char buf[256];
	printf("FrostVistaOS Shell Test. Please type something: \n");

	while (1) {
		printf("fvsh> ");

		while (1) {
			//  Read from fd 0 (stdin/uart)
			int n = read(0, buf, sizeof(buf) - 1);

			if (n > 0) {
				if (buf[0] == '\r') {
					printf("\r\n");
					break;
				}
				buf[n] = '\0'; // Ensure the string ends
				printf("%s", buf);

				int len = strlen(buf);
			} else if (n < 0) {
				printf("Read error!\n");
				break;
			}
		}
	}

	printf("Echo test finished.\n");
	shutdown();
}
