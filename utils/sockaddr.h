#ifndef __UTILS_SOCKADDR_H
#define __UTILS_SOCKADDR_H

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/un.h>

#include "typeinfo.h"

extern int is_valid_addr(const struct sockaddr* addr, socklen_t salen);
extern int is_multicast_addr(const struct sockaddr* addr, socklen_t salen);

#define IN_IS_ADDR_MULTICAST(a) ((a & 255) >= 224 && (a & 255) <= 239)

int socklen(const struct sockaddr* addr);
int sockcmp(const struct sockaddr* addr1, const struct sockaddr* addr2);
int sockcpy(struct sockaddr* dest, const struct sockaddr* src);
int sockncpy(struct sockaddr* dest, socklen_t dest_len, const struct sockaddr* src, socklen_t src_len);
int getsockport(const struct sockaddr* sa);
int setsockport(const struct sockaddr* sa, int port);
const char* sock_ntop_host(const struct sockaddr* sa, socklen_t salen);
int sock_aton(const char* s, struct sockaddr* sa, socklen_t* salen);

TYPEDECL(sockaddr);

#endif
