#ifndef RIME_UNISTD_H
#define RIME_UNISTD_H

void *sbrk(int increment);
int execv(const char *path, char *const argv[]);
unsigned int sleep(unsigned int seconds);
int unlink(const char *path);

#endif
