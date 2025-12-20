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

  if (d == s) {
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

long strlen(const char *str) {
  long cnt = 0;
  while (*str++ != '\0')
    cnt++;
  return cnt;
}
