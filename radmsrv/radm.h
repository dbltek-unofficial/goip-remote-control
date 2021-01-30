#ifndef __RADM__H
#define __RADM__H

#include "rc4.h"
#include "time.h"
#include <errno.h>

#define MYDLOG(x...)                  \
    if (errno == 24)                  \
        exit(1);                      \
    else if (errlog) {                \
        mytime = time(0);             \
        DPRINT("%s", ctime(&mytime)); \
        DLOG(x);                      \
    }
#define RADMIN 1
#define RLOGIN 2
#define TELNET 3

time_t mytime;
struct radmcli {
    struct radmcli* next;
    struct radmsrv* srv;
    char id[256];
    int l_sock;
    int r_sock;
    int is_radmin;
    struct sockaddr_in addr;
    struct rc4_state* rc4_dec;
    struct timer* deltimer;
};

struct radmconn {
    struct radmconn* next;
    struct radmsrv* srv;
    char buf[256];
    int buflen;
    int sock;
    struct sockaddr_in addr;
    struct rc4_state* rc4_dec;
};

struct radmsrv {
    int sock;
    struct radmcli* clients;
    struct radmconn* conns;
    char* rc4_key;
    int rc4_keylen;
};

struct radmsrv* radmsrv_create(struct sockaddr_in* addr, char* key);
int radmsrv_del(struct radmsrv* srv, const char* id, int is_radmin);
struct radmcli* radmsrv_find_client(struct radmsrv* srv, const char* id, int IS_RADMIN);

#endif
