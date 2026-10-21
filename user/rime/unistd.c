#include <user/user.h>

void *sbrk(int increment)
{
	uint64 old_brk = brk(0);
	uint64 new_brk = brk((void *) (old_brk + increment));

	if (new_brk != old_brk + increment)
		return (void *) -1;

	return (void *) old_brk;
}

int execv(const char *path, char *const argv[])
{
	return execve(path, argv, 0);
}

unsigned int sleep(unsigned int seconds)
{
	struct linux_timespec req = {
	    .tv_sec = seconds,
	    .tv_nsec = 0,
	};

	return nanosleep(&req) < 0 ? seconds : 0;
}

int unlink(const char *path)
{
	return unlinkat(AT_FDCWD, path, 0);
}
