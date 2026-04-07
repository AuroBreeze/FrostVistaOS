#include "kernel/types.h"
void *memset(void *s, int c, long n) {
  uchar *p = (uchar *)s;
  uchar v = (uchar)c;

  for (long i = 0; i < n; i++) {
    p[i] = v;
  }

  return s;
}

void *memcpy(void *dest, const void *src, long n) {
  uchar *d = (uchar *)dest;
  const uchar *s = (const uchar *)src;

  for (long i = 0; i < n; ++i) {
    *d++ = *s++;
  }

  return dest;
}

void *memmove(void *dest, const void *src, long n) {
  uchar *d = (uchar *)dest;
  const uchar *s = (const uchar *)src;

  if (d == s)
    return dest;

  if (d < s) {
    for (long i = 0; i < n; ++i) {
      *d++ = *s++;
    }
    return dest;
  }

  for (long i = n; i > 0; --i) {
    d[i - 1] = s[i - 1];
  }
  return dest;
}

void strcpy(char *s1, const char *s2) {
  while (*s2 != '\0') {
    *s1++ = *s2++;
  }
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 != '\0' && *s1 == *s2) {
    s1++;
    s2++;
  }
  return (uchar)*s1 - (uchar)*s2;
}

long strlen(const char *str) {
  long cnt = 0;
  while (*str++ != '\0')
    cnt++;
  return cnt;
}
