#ifndef FV_KERNEL_SYSFILE_H
#define FV_KERNEL_SYSFILE_H

#define POLLIN   0x0001 // Data is available to read.
#define POLLPRI  0x0002 // Priority data or an exceptional condition is pending.
#define POLLOUT  0x0004 // Writing is possible without blocking.
#define POLLERR  0x0008 // An error occurred on the file descriptor.
#define POLLHUP  0x0010 // The peer has closed its end of the connection.
#define POLLNVAL 0x0020 // The file descriptor is invalid.

struct pollfd {
    int   fd;         /* file descriptor */
    short events;     /* requested events */
    short revents;    /* returned events */
};

#endif
