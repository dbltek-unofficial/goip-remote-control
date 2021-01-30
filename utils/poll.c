#ifdef _NO_POLL
#include "poll.h"

int poll(struct pollfd* pfd_list, int pfd_count, int timeout)
{
    fd_set rdset, wrset, epset;
    struct timeval tv;
    int i, fdmax = 0, count;

    FD_ZERO(&rdset);
    FD_ZERO(&wrset);
    FD_ZERO(&epset);

    for (i = 0; i < pfd_count; i++) {
        if (pfd_list[i].fd > fdmax)
            fdmax = pfd_list[i].fd;
        if (pfd_list[i].events & POLLIN) {
            FD_SET(pfd_list[i].fd, &rdset);
        }
        if (pfd_list[i].events & POLLOUT) {
            FD_SET(pfd_list[i].fd, &wrset);
        }
        FD_SET(pfd_list[i].fd, &epset);
    }
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    if ((count = select(fdmax + 1, &rdset, &wrset, &epset, timeout < 0 ? 0 : &tv)) < 0) {
        return count;
    }

    for (i = 0; i < pfd_count; i++) {
        if (FD_ISSET(pfd_list[i].fd, &rdset)) {
            pfd_list[i].revents |= POLLIN;
        }
        if (FD_ISSET(pfd_list[i].fd, &wrset)) {
            pfd_list[i].revents |= POLLOUT;
        }
        if (FD_ISSET(pfd_list[i].fd, &epset)) {
            pfd_list[i].revents |= POLLERR;
        }
    }

    return count;
}

#endif
