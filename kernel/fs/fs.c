#include "kernel/fs.h"
#include "kernel/defs.h"

/**
 * skipelem: Return a pointer to the position following the next ‘/’ and copy
 * the current segment into `name`
 *
 * Return: a pointer to the position following the next ‘/’,
 * Return 0 if the path is empty
 * */
char *skipelem(char *path, char *name)
{
	while (*path == '/')
		path++;
	if (*path == '\0')
		return 0;

	char *s = path;
	while (*path != '/' && *path != '\0')
		path++;

	int len = (int) (path - s);
	if (len >= PATH_MAX)
		len = PATH_MAX - 1;
	memmove(name, s, len);
	name[len] = '\0';

	while (*path == '/')
		path++;
	return path;
}

int namecmp(const char *s, const char *t)
{
	return strncmp(s, t, DIRSIZ);
}
