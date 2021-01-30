

#ifndef _TRACE_H_
#define _TRACE_H_

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "in_chksum.h"

#define ERR_SUCCESS 0
#define ERR_UDP_SOCKET_CREATE 1
#define ERR_UDP_SOCKET_BIND 2
#define ERR_UDP_SOCKET_SETOPT 3
#define ERR_UDP_SOCKET_SEND 4
#define ERR_ICMP_SOCKET_CREATE 5
#define ERR_ICMP_SOCKET_SETOPT 6
#define ERR_ICMP_SOCKET_SEND 7
#define ERR_ICMP_SOCKET_RECV 8
#define ERR_TIMEOUT 20

int get_gate_addr(struct in_addr ast_dest_addr, /* give a destination ip address */
    int ai_metric, /* how far of the gateway */
    long al_timeout, /* timeout in millisecond */
    struct in_addr* apst_gate_addr); /* the gateway addr */

#define SEND_BUFFER_SIZE 100
#define RECV_BUFFER_SIZE 1500
#define OPT_BUFFER_SIZE 40
#define DEST_PORT 20000

#define IP_MIN_HEAD_LEN 20
#define ICMP_MIN_HEAD_LEN 8
#define ICMP_ECHO_LEN 56
#define UDP_MIN_HEAD_LEN 8

int get_outside_addr(struct in_addr ast_dest_addr,
    int ai_metric,
    struct timeval* apst_timeout,
    struct in_addr* apst_outside_addr);

int get_gate_addr_ex(struct in_addr ast_outside_addr,
    int ai_metric,
    struct timeval* apst_timeout,
    struct in_addr* apst_gate_addr);
void timeval_sub(struct timeval* apst_time1,
    struct timeval ast_time2);

void timeval_div(struct timeval* apst_time,
    int ai_div);

#endif
