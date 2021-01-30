#ifndef _UTILS_NET_H_
#define _UTILS_NET_H_

#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <unistd.h>

#include "sockaddr.h"

#ifdef __cplusplus
extern "C" {
#endif

int getsockerr(int sock);
int gethostaddr(const char* hostname, struct sockaddr* addr, socklen_t salen);

extern int tcp_connect(int sock, struct sockaddr* addr, socklen_t salen);
extern int tcp_listen(int sock, struct sockaddr* addr, socklen_t salen);
extern int tcp_bind(int sock, struct sockaddr* addr, socklen_t salen);

extern int udp_connect(int sock, struct sockaddr* addr, socklen_t salen);
extern int udp_bind(int sock, struct sockaddr* addr, socklen_t salen);

int unix_listen(int sock, const char* path);
int unix_connect(int sock, const char* path);

struct ctlmsg {
    char name[32];

    int* fds;
    size_t numfds;

    void* data;
    size_t data_len;
};

int send_ctlmsg(int ctlsock, struct ctlmsg* ctlmsg);
int recv_ctlmsg(int ctlsock, struct ctlmsg* ctlmsg);

int broadcast(int sock, void* msg, size_t len, int flags, unsigned port);
int wait_socket(int sock, int event, int timeout);

#define IFI_NAME 16
#define IFI_HADDR 8

#define IFI_ALIASES 0x1
#define IFI_LOOPBACKS 0x2
#define IFI_BRDADDR 0x4
#define IFI_DSTADDR 0x8
#define IFI_NETMASK 0x10

struct ifi_info {
    char ifi_name[IFI_NAME];
    unsigned char ifi_haddr[IFI_HADDR];
    unsigned short ifi_hlen;
    short ifi_flags;
    struct sockaddr* ifi_addr;
    struct sockaddr* ifi_netmask;
    struct sockaddr* ifi_brdaddr;
    struct sockaddr* ifi_dstaddr;
    struct ifi_info* ifi_next;
};

extern struct ifi_info* get_ifi_info(int sock, int flags);
extern void free_ifi_info(struct ifi_info*);
void print_ifi_info(struct ifi_info* ifi);
struct ifi_info* ifi_select(int sock, struct sockaddr* dest);

#define MK_INET_ANYADDR(addr)                                        \
    {                                                                \
        ((struct sockaddr_in*)(addr))->sin_family = AF_INET;         \
        ((struct sockaddr_in*)(addr))->sin_addr.s_addr = INADDR_ANY; \
        ((struct sockaddr_in*)(addr))->sin_port = 0;                 \
    }

int getifaddr(const char* devnam, struct sockaddr* addr, socklen_t* addrlen);
int getsockifaddr(int sock, struct sockaddr* addr, socklen_t* addrlen);
//int getsockname_psudo(int sock, struct sockaddr *addr, socklen_t *addrlen);
//int has_bind_interface(struct sockaddr *addr);
struct sockaddr* detect_gateway_addr(struct sockaddr* addr, socklen_t addrlen);

#ifdef __cplusplus
};
#endif
#endif //_NET_H_
