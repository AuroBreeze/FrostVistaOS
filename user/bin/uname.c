#include "user.h"

void _start(int argc, char **argv)
{
	struct linux_utsname uts;
	long ret = uname(&uts);
	if (ret < 0) {
		printf("uname failed\n");
	}

	printf("    ______                __ _    ___      __       \n");
	printf("   / ____/________  _____/ /| |  / (_)____/ /_____ _\n");
	printf("  / /_  / ___/ __ \\/ ___/ __/ | / / / ___/ __/ __ `/\n");
	printf(" / __/ / /  / /_/ (__  ) /_ | |/ / (__  ) /_/ /_/ / \n");
	printf("/_/   /_/   \\____/____/\\__/ |___/_/____/\\__/\\__,_/\n");
	printf("\n");
	printf("  OS:       %s\n", uts.sysname);
	printf("  Host:     %s\n", uts.nodename);
	printf("  Kernel:   %s\n", uts.release);
	printf("  Version:  %s\n", uts.version);
	printf("  Machine:  %s\n", uts.machine);
	printf("  Domain:   %s\n", uts.domainname);

	exit(0);
}
