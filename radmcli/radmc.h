#ifndef __RADMC__H
#define __RADMC__H

#include "rc4.h"
#include <netinet/in.h>
#include <sys/socket.h>

#define RADMIN 1
#define RLOGIN 0

struct radmcs;

struct radmc {
    struct radmcs* radmcs;
    struct sockaddr_in raddr;
    struct sockaddr_in laddr;
    char* rc4_key;
    int rc4_keylen;
    char* id;
    struct rc4_state* rc4_enc;
    int sock;
    int IS_RADMIN;
    struct timer* timer;
};

struct radmcs {
    struct radmc* radmc_http;
    struct radmc* radmc_telnet;
    struct timer* timer;
};
struct radmc* radmc_create(struct sockaddr_in* raddr, struct sockaddr_in* laddr, char* key, char* id, int type, int timeout);
struct radmcs* radmcs_create(struct sockaddr_in* raddr, struct sockaddr_in* A_laddr, struct sockaddr_in* L_laddr, char* key, char* id, int timeout);
#endif
