#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "sockaddr.h"

#define DBLEVEL 0

TYPEDEF(struct sockaddr_storage, sockaddr, 0, 0, sockcpy);

int socklen(const struct sockaddr* sa)
{
    socklen_t salen;

    switch (sa->sa_family) {
    case AF_INET:
        salen = sizeof(struct sockaddr_in);
        break;
    case AF_UNIX:
        salen = sizeof(struct sockaddr_un);
        break;
    default:
        salen = sizeof(struct sockaddr);
    }
    //testmem(sa, salen);
    return salen;
}

int getsockport(const struct sockaddr* sa)
{
    int port;

    switch (sa->sa_family) {
    case AF_INET: {
        struct sockaddr_in* sin = (struct sockaddr_in*)sa;
        port = ntohs(sin->sin_port);
    } break;
    default:
        return -1;
    }

    return port;
}

int setsockport(const struct sockaddr* sa, int port)
{
    switch (sa->sa_family) {
    case AF_INET: {
        struct sockaddr_in* sin = (struct sockaddr_in*)sa;
        sin->sin_port = htons(port);
    } break;
    default:
        return -1;
    }

    return port;
}

int sockcmp(const struct sockaddr* addr1, const struct sockaddr* addr2)
{
    if (addr1->sa_family != addr2->sa_family) {
        return -1;
    }
    return memcmp(addr1, addr2, socklen(addr1));
}

int sockcpy(struct sockaddr* dest, const struct sockaddr* src)
{
    int src_len;

    if (!is_valid_addr(src, 0)) {
        return -1;
    }
    src_len = socklen(src);
    memcpy(dest, src, src_len);

    return src_len;
}

int sockncpy(struct sockaddr* dest, socklen_t dest_len, const struct sockaddr* src, socklen_t src_len)
{
    if (!is_valid_addr(src, src_len)) {
        return -1;
    }
    src_len = socklen(src);
    if (dest_len < src_len) {
        return -1;
    }
    memcpy(dest, src, src_len);

    return src_len;
}

const char* sock_ntop_host(const struct sockaddr* sa, socklen_t salen)
{
    char portstr[7];
    static char str[128];

    switch (sa->sa_family) {
    case AF_INET: {
        struct sockaddr_in* sin = (struct sockaddr_in*)sa;
        if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
            return NULL;
        if (ntohs(sin->sin_port) != 0) {
            snprintf(portstr, sizeof(portstr), ":%d", ntohs(sin->sin_port));
            strcat(str, portstr);
        }

        return str;
    }
    }

    return 0;
}

static int parse_ipv4_addr(const char* str, struct sockaddr_in* sin)
{
    char* ep;
    unsigned char ip[4];
    unsigned short port = 0;
    long n;
    int i;

    if (isspace(*str))
        str++;

    for (i = 0; i < 4; i++) {
        n = strtol(str, &ep, 10);
        if (str == ep || errno == ERANGE) {
            return -1;
        }
        if ((i < 3 && *ep != '.') || n < 0 || n > 255) {
            return -1;
        }
        str = ep + 1;
        ip[i] = n;
    }

    if (*ep == ':') {
        n = strtol(str, &ep, 10);
        if (str == ep || errno == ERANGE) {
            return -1;
        }
        if (n < 0 || n > 65535) {
            return -1;
        }
        port = n;
    }

    sin->sin_family = AF_INET;
    memcpy(&sin->sin_addr, ip, 4);
    sin->sin_port = htons(port);

    return 0;
}

int sock_aton(const char* s, struct sockaddr* sa, socklen_t* salen)
{
    struct sockaddr_storage addr;

    if (!parse_ipv4_addr(s, (struct sockaddr_in*)&addr)) {
        if (*salen < sizeof(struct sockaddr_in)) {
            return -1;
        }
        memcpy(sa, &addr, sizeof(struct sockaddr_in));
        return 0;
    }

    return -1;
}

int is_valid_addr(const struct sockaddr* sa, socklen_t salen)
{
    switch (sa->sa_family) {
    case AF_INET: {
        if (salen > 0 && salen < sizeof(struct sockaddr_in)) {
            LOG_FUNCTION_FAILED;
            return 0;
        }
        /*
            if(IN_BADCLASS(sin->sin_addr.s_addr)){
                LOG_FUNCTION_FAILED;
                return 0;
            }
            */
    } break;
    default:
        return 0;
    }

    return 1;
}

int is_multicast_addr(const struct sockaddr* sa, socklen_t salen)
{
    if (!is_valid_addr(sa, salen)) {
        return -1;
    }

    if (sa->sa_family == AF_INET) {
        return IN_IS_ADDR_MULTICAST(((struct sockaddr_in*)sa)->sin_addr.s_addr);
    } else {
        return 0;
    }
}
