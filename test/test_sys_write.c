#include "user.h"

int main()
{
	int buf_size = 400;

	// testing with 400 bytes
	char buf[400];

	for (int i = 0; i < buf_size - 1; i++) {
		buf[i] = 'A' + (i % 26); // generate A-Z
	}
	buf[buf_size - 1] = '\n';

	printf("=========================================\n");
	printf("Starting sys_write test with %d bytes...\n", buf_size);
	printf("=========================================\n");

	int ret = write(1, buf, buf_size);

	// 打印测试结果
	printf("\n\n");
	printf("=========================================\n");
	printf("              Test Results               \n");
	printf("=========================================\n");
	printf("Requested to write : %d bytes\n", buf_size);
	printf("Return value       : %d bytes\n", ret);

	if (ret < buf_size) {
		printf("Status: WARNING! Write was truncated by the kernel.\n");
		printf("If return value is %d, the kernel only processed "
		       "partial data.\n",
		       ret);
	} else {
		printf("Status: SUCCESS! All bytes were written.\n");
	}

	exit(0);
}
