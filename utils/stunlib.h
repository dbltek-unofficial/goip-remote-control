#ifndef STUNLIB_H
#define STUNLIB_H

#include <sys/socket.h>

#include "stun.h"

typedef struct stun_handler {
    int udp_socket;
    int tcp_socket;
    int nat_num;
    struct sockaddr_in servaddr;
    struct sockaddr_in nataddr;
    StunAtrString originalusername;
    StunAtrString originalpassword;
    StunAtrString tmpusername;
    StunAtrString tmppassword;
} stun_handler_t;

void stun_handler_init(stun_handler_t* handler);

void set_stun_servaddr(stun_handler_t* handler, const struct sockaddr_in* servaddr, size_t* len);

void set_udp_socket(stun_handler_t* handler, int udp);

void set_tcp_socket(stun_handler_t* handler, int tcp);

int send_bindingreq(stun_handler_t* handler);

int send_shsreq(stun_handler_t* handler);

int recv_bindingresp(stun_handler_t* handler);

int recv_shsresp(stun_handler_t* handler);

void stun_trysend(int fd, int num, const struct sockaddr* addr, size_t len, int repeat);

void set_nat_num(stun_handler_t* handler, int nat);

static inline int get_tcp_socket(const stun_handler_t* handler)
{
    return handler->tcp_socket;
}

static inline int get_udp_socket(const stun_handler_t* handler)
{
    return handler->udp_socket;
}

void stun_getNatAddress(const stun_handler_t* handler, struct sockaddr* naraddr, size_t* len);

#endif
