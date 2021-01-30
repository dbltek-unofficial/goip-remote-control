/*本文件是对设备连接到服务器的处理*/
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "app.h"
#include "debug.h"
#include "fwd.h"
#include "ip_list.h"
#include "net.h"
#include "radm.h"

extern int errlog;
unsigned short port_min = 0;
unsigned short port_max = 0;
unsigned short get_port = 0;

void radmcli_accept_callback(struct radmcli* cli, unsigned events, int fd)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int sock;
    struct rc4_state* rc4_enc;
    unsigned int ip;

    sock = accept(cli->l_sock, (struct sockaddr*)&addr, &addrlen);
    if (sock < 0) {
        MYDLOG("accept error: %m");
        fprintf(stderr, "accept error: %m\n");
        return;
    }

    //*CHECKPOINT;
    ip = inet_addr(inet_ntoa(addr.sin_addr));
    if (!check_white_ip(ip) && get_ip_data(ip) == NULL) {
        close(sock);
        return;
    }
    //*/
    //DLOG("new:%d", sock);

    rc4_enc = malloc(sizeof(struct rc4_state));
    rc4_setup(rc4_enc, cli->srv->rc4_key, cli->srv->rc4_keylen);

    do_forward(cli->r_sock, sock, rc4_enc, cli->rc4_dec); //r_sock是到设备的连接，sock是到客户的连接， 两条连接开始数据中转

    cli->r_sock = -1;
    cli->rc4_dec = NULL;

    unregister_sock(cli->l_sock, cli, radmcli_accept_callback);
}

void radmcli_release(struct radmcli* cli)
{
    stop_timer(cli->deltimer);
    destroy_timer(cli->deltimer);
    unregister_sock(cli->l_sock, cli, radmcli_accept_callback);
    close(cli->l_sock);
    close(cli->r_sock);
    if (cli->rc4_dec)
        free(cli->rc4_dec);
    free(cli);
}

struct radmcli* radmsrv_find_client(struct radmsrv* srv, const char* id, int is_radmin)
{
    struct radmcli* cli = srv->clients;

    while (cli) {
        //fprintf(stderr, "match client %s/%s\n", cli->id, id);
        if (strcmp(cli->id, id) == 0 && cli->is_radmin == is_radmin) {
            fprintf(stderr, "match client %s/%s\n", cli->id, id);
            stop_timer(cli->deltimer);
            start_timer(cli->deltimer, 1);
            return cli;
        }
        cli = cli->next;
    }

    return cli;
}

int radmsrv_alloc_addr(struct radmsrv* srv, struct sockaddr_in* addr)
{

    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;

    if (port_max > 0) {
        addr->sin_port = htons(get_port++);
        if (get_port > port_max)
            get_port = port_min;
    }
    //addr->sin_port=htons(rand()%(port_max-port_min+1));
    return 0;
}

void timeout_del(struct radmcli* cli)
{
    fprintf(stderr, "timeout!\n");
    radmsrv_del(cli->srv, cli->id, cli->is_radmin);
}

struct radmcli* radmsrv_new_client(struct radmsrv* srv, const char* id)
{
    struct radmcli *cli, *tmp;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int sock, i;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        MYDLOG("cannot create socket: %m");
        fprintf(stderr, "cannot create socket: %m\n");
        return NULL;
    }
    DLOG("new %d\n", sock);
    fprintf(stderr, "new %d\n", sock);

    for (i = 0; i < 10; i++) {
        if (radmsrv_alloc_addr(srv, &addr) < 0) {
            close(sock);
            return NULL;
        }
        if (bind(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) { //
            //close(sock);
            MYDLOG("cannot bind address: %s:%d: %m", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            fprintf(stderr, "cannot bind address: %s:%d: %m\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            //return NULL;
        } else
            break;
    }
    if (i >= 10) {
        close(sock);
        return NULL;
    }
    if (listen(sock, 5) < 0) {
        close(sock);
        MYDLOG("%m");
        fprintf(stderr, "cannot listen: %m\n");
        return NULL;
    }

    getsockname(sock, (struct sockaddr*)&addr, &addrlen);
    DLOG("client address: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    fprintf(stderr, "client address: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    cli = malloc(sizeof(struct radmcli));
    memset(cli, 0, sizeof(struct radmcli));

    cli->srv = srv;
    cli->l_sock = sock;
    strncpy(cli->id, id, sizeof(cli->id) - 1);

    tmp = srv->clients;
    if (!tmp || strcmp(tmp->id, id) > 0) {
        cli->next = srv->clients;
        srv->clients = cli;
    } else {
        while (tmp->next) {
            if (strcmp(tmp->next->id, id) > 0) {
                cli->next = tmp->next;
                tmp->next = cli;
                break;
            }
            tmp = tmp->next;
        }
        if (!tmp->next)
            tmp->next = cli;
    }

    //cli->next=srv->clients;
    //srv->clients = cli;

    cli->deltimer = create_timer(30 * 60 * SECOND, cli, timeout_del);
    start_timer(cli->deltimer, 1);
    return cli;
}

void radmsrv_conn_callback(struct radmconn* conn, unsigned events, int fd)
{
    int res;
    struct radmcli* cli;
    int is_radmin;

    if (events & SOCKERR) {
        goto err_exit;
    }

    res = recv(conn->sock, conn->buf + conn->buflen, sizeof(conn->buf) - conn->buflen, 0);
    if (res <= 0) {
        MYDLOG("%m");
        fprintf(stderr, "radmconn recv error: %m\n");
        goto err_exit;
    }

    rc4_crypt(conn->rc4_dec, conn->buf + conn->buflen, res);
    conn->buflen += res;

    if (conn->buf[conn->buflen - 1] != '\n') {
        return;
    }

    conn->buf[conn->buflen - 1] = '\0';

    if (strncmp(conn->buf, "RADMIN: ", 8) == 0) //网页
        is_radmin = 1;
    else if (strncmp(conn->buf, "RLOGIN: ", 8) == 0) //以前的接口，不再使用
        is_radmin = 2;
    else if (strncmp(conn->buf, "TELNET: ", 8) == 0) //telnet
        is_radmin = 3;
    else
        goto err_exit;

    //DLOG("recv sock:%d", conn->sock);
    DLOG("id:%s", conn->buf + 8);
    cli = radmsrv_find_client(conn->srv, conn->buf + 8, is_radmin);
    if (!cli) {
        cli = radmsrv_new_client(conn->srv, conn->buf + 8);
    }
    if (!cli) {
        goto err_exit;
    }

    if (cli->r_sock > 0) {
        //DLOG("close:%d, ti:%d", cli->r_sock, conn->sock);
        close(cli->r_sock);
    }
    if (cli->rc4_dec) {
        free(cli->rc4_dec);
    }

    cli->r_sock = conn->sock; //r_sock是连接到设备
    memcpy(&cli->addr, &conn->addr, sizeof(struct sockaddr_in));
    cli->is_radmin = is_radmin;
    cli->rc4_dec = conn->rc4_dec;
    register_sock(cli->l_sock, SOCKIN, cli, radmcli_accept_callback); //l_sock是开放给客户，地址跟端口显示在页面上
    unregister_sock(conn->sock, conn, radmsrv_conn_callback);
    free(conn);

    return;

err_exit:
    unregister_sock(conn->sock, conn, radmsrv_conn_callback);
    close(conn->sock);
    free(conn->rc4_dec);
    free(conn);
}

void radmsrv_accept_callback(struct radmsrv* srv, unsigned events, int fd)
{
    int sock;
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    struct radmconn* conn;

    if ((sock = accept(srv->sock, (struct sockaddr*)&from, &fromlen)) < 0) {
        MYDLOG("%m");
        fprintf(stderr, "accept failed: %m\n");
        return;
    }
    //DLOG("new:%d, prot:%d", sock, from.sin_port);
    conn = malloc(sizeof(struct radmconn));
    memset(conn, 0, sizeof(struct radmconn));
    conn->sock = sock;
    conn->srv = srv;
    memcpy(&conn->addr, &from, sizeof(struct sockaddr_in));
    conn->rc4_dec = malloc(sizeof(struct rc4_state));
    rc4_setup(conn->rc4_dec, srv->rc4_key, srv->rc4_keylen);
    register_sock(sock, SOCKIN, conn, radmsrv_conn_callback);
}

struct radmsrv* radmsrv_create(struct sockaddr_in* addr, char* key)
{
    struct radmsrv* srv;
    int sock;

    if ((sock = tcp_bind(-1, (struct sockaddr*)addr, sizeof(struct sockaddr_in))) < 0) {
        MYDLOG("%m");
        fprintf(stderr, "cannot bind address: %s:%d: %m\n", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
        return NULL;
    }

    if (listen(sock, 5) < 0) {
        MYDLOG("%m");
        fprintf(stderr, "cannot listen: %m\n");
        return NULL;
    }

    srv = malloc(sizeof(struct radmsrv));
    memset(srv, 0, sizeof(struct radmsrv));

    srv->sock = sock;
    srv->rc4_key = strdup(key);
    srv->rc4_keylen = strlen(key);

    register_sock(sock, SOCKIN, srv, radmsrv_accept_callback);

    ip_list_init();
    return srv;
}

int radmsrv_del(struct radmsrv* radmsrv, const char* id, int is_radmin)
{
    struct radmcli* cli;

    cli = radmsrv->clients;

    if (strcmp(cli->id, id) == 0 && cli->is_radmin == is_radmin) {
        radmsrv->clients = cli->next;
        radmcli_release(cli);
        return 0;
    } else {
        while (cli->next) {
            if (strcmp(cli->next->id, id) == 0 && cli->next->is_radmin == is_radmin) {
                struct radmcli* tmp;

                tmp = cli->next;
                cli->next = tmp->next;
                radmcli_release(tmp);
                return 0;
            }
            cli = cli->next;
        }
    }

    return -1;
}
