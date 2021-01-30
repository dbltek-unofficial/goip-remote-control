/* 处理IP白名单，不在名单上的IP禁止访问 */
#include "ip_list.h"
#include "datalist.h"
#include "debug.h"
#include "timer.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

char* get_ip_string(unsigned int ip)
{
    struct in_addr a;
    a.s_addr = ip;
    return inet_ntoa(a);
}

int check_white_ip(unsigned int ip)
{
    char ip_buf[256];
    char *p, *ip_s = ip_buf;
    strncpy(ip_buf, white_list_ip, sizeof(ip_buf));
    //fprintf(stderr, "white_list_ip:%s\n",white_list_ip);
    if ((p = strchr(ip_s, '|')) != NULL) {
        *p = 0;
        if (!strcmp(get_ip_string(ip), ip_s))
            return 1;
        ip_s = ++p;
    }
    if (!strcmp(get_ip_string(ip), ip_s))
        return 1;
    return 0;
}

struct ip_data* get_ip_data(unsigned int ip)
{
    struct datalist_iterator* it;
    fprintf(stderr, "%p, %d, try get ip:%s\n", ip_list, datalist_size(ip_list), get_ip_string(ip));
    for (it = datalist_first(ip_list); it;) {
        struct ip_data* ip_data = datalist_itdata(it);
        fprintf(stderr, "check get ip:%s\n", get_ip_string(ip_data->ip));
        if (ip_data->ip == ip) {
            fprintf(stderr, "get ip:%s\n", get_ip_string(ip));
            return ip_data;
        }
        it = datalist_next(it);
    }
    return NULL;
}

void ip_timeout(struct ip_data* ip_data)
{
    fprintf(stderr, "del ip:%s\n", get_ip_string(ip_data->ip));
    datalist_remove_data(ip_list, ip_data);
    destroy_timer(ip_data->timer);
    free(ip_data);
}

void del_ip(unsigned int ip)
{
    struct datalist_iterator* it;
    for (it = datalist_first(ip_list); it;) {
        struct ip_data* ip_data = datalist_itdata(it);
        it = datalist_next(it);
        if (ip_data->ip == ip) {
            ip_timeout(ip_data);
        }
    }
}

void add_ip(unsigned int ip)
{
    struct ip_data* ip_data = get_ip_data(ip);

    fprintf(stderr, "try add ip:%s\n", get_ip_string(ip));
    if (ip_data) {
        stop_itimer(ip_data->timer);
    } else {
        ip_data = malloc(sizeof(struct ip_data));
        ip_data->ip = ip;
        ip_data->timer = create_timer(60 * 60 * 24 * 1000, ip_data, ip_timeout);
        datalist_append(ip_list, ip_data);
        fprintf(stderr, "add ip:%s\n", get_ip_string(ip));
    }
    start_timer(ip_data->timer, 1);
}

void ip_list_init()
{
    ip_list = create_datalist();
    datalist_init(ip_list);
}
