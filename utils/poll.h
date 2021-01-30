#ifdef _NO_POLL

#ifndef __POLL__H
#define __POLL__H

#define POLLIN 0x0001 /* There is data to read */
#define POLLPRI 0x0002 /* There is urgent data to read */
#define POLLOUT 0x0004 /* Writing now will not block */
#define POLLERR 0x0008 /* Error condition */
#define POLLHUP 0x0010 /* Hung up */
#define POLLNVAL 0x0020 /* Invalid request: fd not open */

struct pollfd {
    int fd; /* file descriptor */
    short events; /* requested events */
    short revents; /* returned events */
};

int poll(struct pollfd* pfd_list, int pfd_count, int timeout);

#endif

#else

#include <sys/poll.h>

#endif
