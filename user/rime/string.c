#include <user/rime/string.h>

unsigned long strlen(const char *s)
{
	unsigned long n = 0;
	while (s[n])
		n++;
	return n;
}

void *memcpy(void *dst, const void *src, unsigned long n)
{
	char *d = dst;
	const char *s = src;

	for (unsigned long i = 0; i < n; i++)
		d[i] = s[i];

	return dst;
}

void *memset(void *dst, int c, unsigned long n)
{
	char *d = dst;

	for (unsigned long i = 0; i < n; i++)
		d[i] = c;

	return dst;
}

int strcmp(const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	return (unsigned char) *s1 - (unsigned char) *s2;
}

int strncmp(const char *s1, const char *s2, unsigned long n)
{
	while (n > 0 && *s1 && *s1 == *s2) {
		s1++;
		s2++;
		n--;
	}
	return n == 0 ? 0 : (unsigned char) *s1 - (unsigned char) *s2;
}

char *strcpy(char *dst, const char *src)
{
	char *start = dst;
	while ((*dst++ = *src++) != '\0')
		;
	return start;
}

char *strncpy(char *dst, const char *src, unsigned long n)
{
	unsigned long i;
	for (i = 0; i < n && src[i] != '\0'; i++)
		dst[i] = src[i];
	for (; i < n; i++)
		dst[i] = '\0';
	return dst;
}

char *strchr(const char *s, int c)
{
	while (*s != '\0') {
		if (*s == (char) c)
			return (char *) s;
		s++;
	}
	if (c == '\0')
		return (char *) s;
	return 0;
}
