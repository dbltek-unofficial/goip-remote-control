
#include <netinet/ip_icmp.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "trace.h"

/* static definition */
static pid_t gt_pid;
static long gl_send_seq;

int get_gate_addr(struct in_addr ast_dest_addr, /* give a destination ip address */
    int ai_metric, /* how far of the gateway */
    long al_timeout, /* timeout in millisecond */
    struct in_addr* apst_gate_addr) /* the gateway addr */
{
    struct timeval lst_timeout;
    struct in_addr lst_outside_addr;
    int li_result, i;

    gt_pid = getpid();

    i = 0;
    li_result = ERR_TIMEOUT;
    while ((li_result == ERR_TIMEOUT) && (ai_metric + i < 9)) {
        lst_timeout.tv_sec = al_timeout / 1000;
        lst_timeout.tv_usec = (al_timeout % 1000) * 1000;

        li_result = get_outside_addr(ast_dest_addr,
            ai_metric + i,
            &lst_timeout,
            &lst_outside_addr);
        i++;
    }
    if (li_result != 0)
        return li_result;

    li_result = get_gate_addr_ex(lst_outside_addr,
        ai_metric,
        &lst_timeout,
        apst_gate_addr);

    return li_result;
}

int get_outside_addr(struct in_addr ast_dest_addr,
    int ai_metric,
    struct timeval* apst_timeout,
    struct in_addr* apst_outside_addr)
{
    int li_udp_socket,
        li_icmp_socket,
        li_sockaddr_in_len,
        li_ttl,
        li_quit1,
        li_quit2,
        li_result,
        li_byte_recv,
        li_ip_head_len1,
        li_ip_head_len2,
        i;
    char ls_recv_buffer[RECV_BUFFER_SIZE];
    uint16_t lui_src_port,
        lui_dest_port;
    struct sockaddr_in lst_send_sockaddr_in,
        lst_recv_sockaddr_in;
    struct timeval lst_start_time,
        lst_end_time,
        lst_timeout,
        lst_timeout_tmp;
    struct ip *lpst_ip1, *lpst_ip2;
    struct icmphdr* lpst_icmp;
    struct udphdr* lpst_udp_header;
    fd_set lt_fset;

    /* creating the udp socket and setting its relative options */
    if ((li_udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        close(li_udp_socket);
        return ERR_UDP_SOCKET_CREATE;
    }
    /* set the source port related to the proccess id */
    /* and bind the source port with the udp socket */
    lui_src_port = (gt_pid & 0xffff) | 0x8000;
    lst_send_sockaddr_in.sin_family = AF_INET;
    lst_send_sockaddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    lst_send_sockaddr_in.sin_port = htons(lui_src_port);
    li_sockaddr_in_len = sizeof(struct sockaddr_in);

    if (bind(li_udp_socket,
            (struct sockaddr*)&lst_send_sockaddr_in,
            li_sockaddr_in_len)
        != 0) {
        close(li_udp_socket);
        return ERR_UDP_SOCKET_BIND;
    }
    /* setting the ttl of udp socket */
    li_ttl = ai_metric + 1;

    if (setsockopt(li_udp_socket, IPPROTO_IP, IP_TTL, &li_ttl, sizeof(int)) != 0) {
        close(li_udp_socket);
        return ERR_UDP_SOCKET_SETOPT;
    }

    /* create the icmp socket */
    if ((li_icmp_socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) {
        close(li_udp_socket);
        close(li_icmp_socket);
        return ERR_ICMP_SOCKET_CREATE;
    }

    /* start to find the first node outside NAT */
    li_quit1 = 0; /* 0 - running 1 - found 2 - timeout */
    i = 0;
    lst_send_sockaddr_in.sin_addr = ast_dest_addr;
    FD_ZERO(&lt_fset);

    while ((i < 3) && (li_quit1 == 0)) /* send udp probe packet three times */
    {
        lui_dest_port = DEST_PORT + i; /* every time to a different port */
        lst_send_sockaddr_in.sin_port = htons(lui_dest_port);
        li_sockaddr_in_len = sizeof(struct sockaddr_in);

        /* send udp probe packet */
        if (sendto(li_udp_socket,
                &i,
                sizeof(int),
                0,
                (struct sockaddr*)&lst_send_sockaddr_in,
                li_sockaddr_in_len)
            != sizeof(int)) {
            close(li_udp_socket);
            close(li_icmp_socket);
            return ERR_UDP_SOCKET_SEND;
        }

        li_quit2 = 0;
        lst_timeout_tmp = *apst_timeout;
        timeval_div(&lst_timeout_tmp, 3 - i);

        while (li_quit2 == 0) /* receive icmp packet until found the needed icmp packet */
        { /* or timeout */
            FD_SET(li_icmp_socket, &lt_fset);

            if ((lst_timeout_tmp.tv_sec < apst_timeout->tv_sec) || ((lst_timeout_tmp.tv_sec == apst_timeout->tv_sec) && (lst_timeout_tmp.tv_usec < apst_timeout->tv_usec)))
                lst_timeout = lst_timeout_tmp;
            else
                lst_timeout = *apst_timeout;

            gettimeofday(&lst_start_time, NULL);
            li_result = select(li_icmp_socket + 1,
                &lt_fset,
                NULL,
                NULL,
                &lst_timeout);
            gettimeofday(&lst_end_time, NULL);
            timeval_sub(&lst_end_time, lst_start_time);
            timeval_sub(apst_timeout, lst_end_time);
            timeval_sub(&lst_timeout_tmp, lst_end_time);

            if (li_result < 0) /* error */
            {
                close(li_udp_socket);
                close(li_icmp_socket);
                return ERR_ICMP_SOCKET_RECV;
            } else if (li_result == 0) /* timeout */
            {
                i++;
                li_quit2 = 1;
            } else if (FD_ISSET(li_icmp_socket, &lt_fset)) /* start to receive icmp packet */
            {
                li_byte_recv = recvfrom(li_icmp_socket,
                    ls_recv_buffer,
                    RECV_BUFFER_SIZE,
                    0,
                    (struct sockaddr*)&lst_recv_sockaddr_in,
                    &li_sockaddr_in_len);
                if (li_byte_recv <= 0) /* error */
                {
                    close(li_udp_socket);
                    close(li_icmp_socket);
                    return ERR_ICMP_SOCKET_RECV;
                }

                lpst_ip1 = (struct ip*)ls_recv_buffer;
                li_ip_head_len1 = lpst_ip1->ip_hl << 2;

                if ((li_byte_recv - li_ip_head_len1) > ICMP_MIN_HEAD_LEN) /* correct icmp packet */
                {
                    lpst_icmp = (struct icmp*)(ls_recv_buffer + li_ip_head_len1);
                    if (((lpst_icmp->type == ICMP_TIMXCEED) && (lpst_icmp->code == ICMP_TIMXCEED_INTRANS)) || ((lpst_icmp->type == ICMP_UNREACH) && (lpst_icmp->code == ICMP_UNREACH_PORT))) /* desired icmp type & code */
                    {
                        if ((li_byte_recv - li_ip_head_len1 - ICMP_MIN_HEAD_LEN) > IP_MIN_HEAD_LEN) /* correct icmp packet */
                        {
                            lpst_ip2 = (struct ip*)(ls_recv_buffer + li_ip_head_len1 + ICMP_MIN_HEAD_LEN);
                            li_ip_head_len2 = lpst_ip2->ip_hl << 2;
                            if (((li_byte_recv - li_ip_head_len1 - ICMP_MIN_HEAD_LEN - li_ip_head_len2) >= UDP_MIN_HEAD_LEN) && (lpst_ip2->ip_p == IPPROTO_UDP)) /* udp packet return message */
                            {
                                lpst_udp_header = (struct udphdr*)(ls_recv_buffer + li_ip_head_len1 + ICMP_MIN_HEAD_LEN + li_ip_head_len2);
                                if ((lpst_udp_header->source == htons(lui_src_port)) && (lpst_udp_header->dest == htons(lui_dest_port))) /* if it is sent by our process */
                                {
                                    *apst_outside_addr = lst_recv_sockaddr_in.sin_addr;
                                    close(li_udp_socket);
                                    close(li_icmp_socket);
                                    return 0;
                                }
                            }
                        }
                    } /* icmp type & icmp code */
                } /* icmp message */
            } /* ip packet read */
        } /* while  -- keep read icmp message */
    } /* while - send upd probe */
    close(li_udp_socket);
    close(li_icmp_socket);
    return ERR_TIMEOUT;
}

int get_gate_addr_ex(struct in_addr ast_outside_addr,
    int ai_metric,
    struct timeval* apst_timeout,
    struct in_addr* apst_gate_addr)
{
    char ls_option[OPT_BUFFER_SIZE],
        ls_send_buffer[SEND_BUFFER_SIZE],
        ls_recv_buffer[RECV_BUFFER_SIZE];
    int li_pos,
        li_icmp_socket,
        //li_byte_send,
        li_byte_recv,
        li_sockaddr_in_len,
        li_ip_header_len,
        li_quit1,
        li_quit2,
        li_result,
        li_ptr,
        i;
    struct sockaddr_in lst_send_sockaddr_in,
        lst_recv_sockaddr_in;
    struct icmphdr* lpst_icmp;
    struct ip* lpst_ip;
    struct timeval lst_start_time,
        lst_end_time,
        lst_timeout,
        lst_timeout_tmp;
    //struct in_addr lst_temp_addr;
    fd_set lt_fset;

    if ((li_icmp_socket = socket(AF_INET,
             SOCK_RAW,
             IPPROTO_ICMP))
        == -1) {
        close(li_icmp_socket);
        return ERR_ICMP_SOCKET_CREATE;
    }

    memset(ls_option, 0, OPT_BUFFER_SIZE);
    li_pos = 0;
    ls_option[li_pos++] = IPOPT_NOP;
    ls_option[li_pos++] = IPOPT_RR;
    ls_option[li_pos++] = OPT_BUFFER_SIZE - 1; /* substract the first byte */
    ls_option[li_pos++] = 4;

    if (setsockopt(li_icmp_socket,
            IPPROTO_IP,
            IP_OPTIONS,
            ls_option,
            OPT_BUFFER_SIZE)
        < 0) {
        close(li_icmp_socket);
        return ERR_ICMP_SOCKET_SETOPT;
    }

    memset(ls_send_buffer, 0, SEND_BUFFER_SIZE);

    lpst_icmp = (struct icmphdr*)ls_send_buffer;

    lpst_icmp->type = ICMP_ECHO;
    lpst_icmp->code = 0;
    lpst_icmp->un.echo.id = getpid();

    lst_send_sockaddr_in.sin_family = AF_INET;
    lst_send_sockaddr_in.sin_addr = ast_outside_addr;

    li_quit1 = 0;
    i = 0;
    FD_ZERO(&lt_fset);

    while ((i < 3) && (li_quit1 == 0)) {
        lpst_icmp->un.echo.sequence = gl_send_seq++;
        lpst_icmp->checksum = 0;
        lpst_icmp->checksum = in_chksum((unsigned short*)lpst_icmp, ICMP_ECHO_LEN);

        li_sockaddr_in_len = sizeof(struct sockaddr_in);
        if (sendto(li_icmp_socket,
                ls_send_buffer,
                ICMP_ECHO_LEN,
                0,
                (struct sockaddr*)&lst_send_sockaddr_in,
                li_sockaddr_in_len)
            != ICMP_ECHO_LEN) {
            close(li_icmp_socket);
            return ERR_ICMP_SOCKET_SEND;
        }

        li_quit2 = 0;
        lst_timeout_tmp = *apst_timeout;
        timeval_div(&lst_timeout_tmp, 3 - i);

        while (li_quit2 == 0) {
            FD_SET(li_icmp_socket, &lt_fset);
            if ((lst_timeout_tmp.tv_sec < apst_timeout->tv_sec) || ((lst_timeout_tmp.tv_sec == apst_timeout->tv_sec) && (lst_timeout_tmp.tv_usec < apst_timeout->tv_usec)))
                lst_timeout = lst_timeout_tmp;
            else
                lst_timeout = *apst_timeout;

            gettimeofday(&lst_start_time, NULL);
            li_result = select(li_icmp_socket + 1,
                &lt_fset,
                NULL,
                NULL,
                &lst_timeout);
            gettimeofday(&lst_end_time, NULL);
            timeval_sub(&lst_end_time, lst_start_time);
            timeval_sub(apst_timeout, lst_end_time);
            timeval_sub(&lst_timeout_tmp, lst_end_time);

            if (li_result < 0) {
                close(li_icmp_socket);
                return ERR_ICMP_SOCKET_RECV;
            } else if (li_result == 0) {
                i++;
                li_quit2 = 1;
            } else if (FD_ISSET(li_icmp_socket, &lt_fset)) {
                li_byte_recv = recvfrom(li_icmp_socket,
                    ls_recv_buffer,
                    RECV_BUFFER_SIZE,
                    0,
                    (struct sockaddr*)&lst_recv_sockaddr_in,
                    &li_sockaddr_in_len);
                if (li_byte_recv < 0) {
                    close(li_icmp_socket);
                    return ERR_ICMP_SOCKET_RECV;
                }

                lpst_ip = (struct ip*)ls_recv_buffer;
                li_ip_header_len = lpst_ip->ip_hl << 2;

                if ((li_ip_header_len > IP_MIN_HEAD_LEN) && ((li_byte_recv - li_ip_header_len) >= ICMP_MIN_HEAD_LEN)) {
                    lpst_icmp = (struct icmp*)(ls_recv_buffer + li_ip_header_len);
                    if ((lpst_icmp->type == ICMP_ECHOREPLY) && (lpst_icmp->un.echo.id == gt_pid)) {
                        li_pos = IP_MIN_HEAD_LEN;
                        while ((li_pos < li_ip_header_len) && (ls_recv_buffer[li_pos] == IPOPT_NOP))
                            li_pos++;
                        if (ls_recv_buffer[li_pos] == IPOPT_RR) {
                            li_ptr = ls_recv_buffer[li_pos + 2] - 1;
                            if (((li_ptr - 3) / 4) > ai_metric) {
                                memcpy(apst_gate_addr, ls_recv_buffer + li_pos + 3 + ai_metric * 4, sizeof(struct in_addr));
                                close(li_icmp_socket);
                                return 0;
                            }
                        } /* if - ip options record route */
                    } /* if - icmp type */
                } /* if - packet length */
            } /* if - select result */
        } /* while - receive icmp packet */
    } /* while - send icmp packet */
    close(li_icmp_socket);
    return ERR_TIMEOUT;
}

void timeval_sub(struct timeval* apst_time1,
    struct timeval ast_time2)
{
    if ((apst_time1->tv_sec < ast_time2.tv_sec) || ((apst_time1->tv_sec == ast_time2.tv_sec) && (apst_time1->tv_usec <= ast_time2.tv_usec))) {
        apst_time1->tv_sec = 0;
        apst_time1->tv_usec = 0;
        return;
    } else if (apst_time1->tv_usec < ast_time2.tv_usec) {
        apst_time1->tv_sec = apst_time1->tv_sec - ast_time2.tv_sec - 1;
        apst_time1->tv_usec = 1000000 + apst_time1->tv_usec - ast_time2.tv_usec;
        return;
    } else {
        apst_time1->tv_sec = apst_time1->tv_sec - ast_time2.tv_sec;
        apst_time1->tv_usec = apst_time1->tv_usec - ast_time2.tv_usec;
        return;
    }
}

void timeval_div(struct timeval* apst_time,
    int ai_div)
{
    int li_temp;

    if (ai_div == 0)
        return;

    li_temp = apst_time->tv_sec % ai_div;
    apst_time->tv_sec = apst_time->tv_sec / ai_div;
    apst_time->tv_usec = (li_temp * 1000000 + apst_time->tv_usec) / ai_div;
}
