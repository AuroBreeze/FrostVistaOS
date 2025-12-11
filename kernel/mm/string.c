#include <stddef.h>
void *memset(void *s, int c, size_t n) {
  unsigned char *p = (unsigned char *)s;
  unsigned char v = (unsigned char)c;

  for (size_t i = 0; i < n; i++) {
    p[i] = v;
  }

  return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;

  for (size_t i = 0; i < n; ++i) {
    *d++ = *s++;
  }

  return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;

  if (d == s)
    return dest;

  if (d == s) {
    for (size_t i = 0; i < n; ++i) {
      *d++ = *s++;
    }
    return dest;
  }

  for (size_t i = n; i > 0; --i) {
    d[i - 1] = s[i - 1];
  }
  return dest;
}

size_t strlen(const char *str) {
  size_t cnt = 0;
  while (*str++ != '\0')
    cnt++;
  return cnt;
}
