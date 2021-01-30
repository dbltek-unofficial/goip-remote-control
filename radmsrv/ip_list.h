#ifndef __IP_LIST_H
#define __IP_LIST_H

struct ip_data {
    unsigned int ip;
    struct timer* timer;
};
struct datalist* ip_list;
char white_list_ip[256];

struct ip_data* get_ip_data(unsigned int ip);
int check_white_ip(unsigned int ip);
void add_ip(unsigned int ip);
void del_ip(unsigned int ip);
void ip_list_init();
#endif
