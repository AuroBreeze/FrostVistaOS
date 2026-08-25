#ifndef STRING_H
#define STRING_H

// string.c
#include "kernel/types.h"

void *memset(void *s, int c, long n);
void *memcpy(void *dest, const void *src, long n);
void *memmove(void *dest, const void *src, long n);
char *strncpy(char *s, const char *t, int n);
void strcpy(char *dst, const char *src);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *p, const char *q, uint n);
long strlen(const char *str);

#endif
