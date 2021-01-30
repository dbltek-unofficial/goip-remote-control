#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "app.h"
#include "net.h"
#include "socks.h"

#include "debug.h"

ssize_t readn(int ai_fd, void* ap_buf, size_t at_count)
{
    ssize_t lt_read;
    size_t lt_left;
    char* ls_ptr;

    ls_ptr = (char*)ap_buf;
    lt_left = at_count;
    while (lt_left > 0) {
        if ((lt_read = recv(ai_fd, ls_ptr, lt_left, 0)) < 0) {
            if (errno == EINTR)
                lt_read = 0;
            else
                return -1;
        } else if (lt_read == 0)
            break;
        lt_left -= lt_read;
        ls_ptr += lt_read;
    }

    return (at_count - lt_left);
}

ssize_t writen(int ai_fd, void* ap_buf, size_t at_count)
{
    ssize_t lt_written;
    size_t lt_left;
    char* ls_ptr;

    ls_ptr = (char*)ap_buf;
    lt_left = at_count;

    while (lt_left > 0) {
        if ((lt_written = send(ai_fd, ls_ptr, lt_left, 0)) <= 0) {
            if (errno == EINTR)
                lt_written = 0;
            else
                return -1;
        }

        lt_left -= lt_written;
        ls_ptr += lt_written;
    }

    return at_count;
}

int socks_auth_rfc1929(int sock) /*authentication method 2*/
{
    const char *usr, *pass;
    unsigned char buf[515];
    int usrlen, passlen;

    if (!(usr = app_getopt("relay-user", 0))) {
        return -1;
    }

    if (!(pass = app_getopt("relay-pwd", 0))) {
        return -1;
    }

    usrlen = strlen(usr);
    passlen = strlen(pass);
    if (usrlen > 255 || passlen > 255) {
        return -1;
    }

    buf[0] = 0x1;
    buf[1] = usrlen;
    memcpy(buf + 2, usr, buf[1]);
    buf[2 + usrlen] = passlen;
    memcpy(buf + usrlen + 3, pass, passlen);

    if (writen(sock, buf, 3 + usrlen + passlen) < 0) {
        LOG_SYS_ERROR;
        return -1;
    }

    if (readn(sock, buf, 2) < 0) { /*the serve verifies the password and username*/
        LOG_SYS_ERROR;
        return -1;
    }

    if (buf[1] != 0) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    return 0;
}

int socks_auth(int sock, int method) /*processing authentication method 2*/
{
    switch (method) {
    case 0:
        return 0;
    case 2:
        return socks_auth_rfc1929(sock);
    }

    DPRINT("%s(): unsupported method: %d\n", __FUNCTION__, method);
    return -1;
}

int socks_get_server_addr(struct sockaddr* addr, socklen_t* addrlen)
{
    struct sockaddr_in* inaddr = (struct sockaddr_in*)addr;
    const char* opt;

    if (!(opt = app_getopt("relay-server", 0))) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    if (*addrlen < sizeof(struct sockaddr_in)) {
        LOG_FUNCTION_FAILED;
        return -1;
    }
    *addrlen = sizeof(struct sockaddr_in);

    inaddr->sin_family = AF_INET;
    if (strspn(opt, "1234567890.") == strlen(opt)) {
        if (inet_aton(opt, &inaddr->sin_addr) < 0) {
            LOG_FUNCTION_FAILED;
            return -1;
        }
    } else {
        if (gethostaddr(opt, (struct sockaddr*)inaddr, sizeof(inaddr)) < 0) {
            LOG_FUNCTION_FAILED;
            return -1;
        }
    }

    if ((opt = app_getopt("relay-port", 0))) {
        inaddr->sin_port = htons(atoi(opt));
    } else {
        inaddr->sin_port = htons(1080);
    }

    return 0;
}

int socks_establish(int sock) /*connect to the socks5 server and send negotiation message*/
{
    const unsigned char msg[] = { 0x05, 1, 0x02 };
    struct sockaddr_in socks_serv;
    socklen_t socks_servlen = sizeof(socks_serv);
    unsigned char rep[2];
    int created = 0;
    fd_set /*rset,*/ wset; /*non-blocking TCP connect implemmented by sam*/
    struct timeval time_out;
    int flags, retval;

    if (socks_get_server_addr((struct sockaddr*)&socks_serv, &socks_servlen) < 0) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    if (sock < 0) {
        if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
            LOG_SYS_ERROR;
            return -1;
        }
        created = 1;
    }

    DPRINT("%s(): connecting to server: %s\n", __FUNCTION__, sock_ntop_host((struct sockaddr*)&socks_serv, sizeof(socks_serv)));

    if ((flags = fcntl(sock, F_GETFL, 0)) == -1) {
        LOG_SYS_ERROR;
        if (created)
            close(sock);
        return -1;
    }

    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        LOG_SYS_ERROR;
        if (created)
            close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&socks_serv, sizeof(socks_serv)) < 0) {
        if (errno != EINPROGRESS) { /*NON BLOCKING option returns this error immediately, we ignore this error*/
            if (created)
                close(sock);
            return -1;
        }
    }

    //	FD_ZERO(&rset);
    FD_ZERO(&wset);
    FD_SET(sock, &wset);

    time_out.tv_sec = 15;
    time_out.tv_usec = 0;

    if ((retval = select(sock + 1, NULL, &wset, NULL, &time_out)) <= 0) {
        DLOG("select timed out or error!!, error: %d", retval);
        if (created)
            close(sock);
        exit(0); /*excuting rest of codes is meaningless, we exit at here*/
        return -1;
    }

    if (fcntl(sock, F_SETFL, flags) == -1) { /*restore file status flags, otherwise the following read and write action
											will return error*/
        LOG_SYS_ERROR;
        if (created)
            close(sock);
        return -1;
    }

    if (writen(sock, &msg, sizeof(msg)) < 0) {
        LOG_SYS_ERROR;
        if (created)
            close(sock);
        return -1;
    }

    if (readn(sock, rep, 2) < 0) { /*get reply message from the server*/
        LOG_SYS_ERROR;
        if (created)
            close(sock);
        return -1;
    }

    if (socks_auth(sock, rep[1]) < 0) { /*processing authentication*/
        LOG_FUNCTION_FAILED;
        if (created)
            close(sock);
        return -2;
    }

    return sock;
}

int socks_cmd(int sock, int cmd, struct sockaddr_in* addr) /*send socks service request and get the reply message*/
{
    unsigned char buf[22];

    buf[0] = 0x05;
    buf[1] = cmd;
    buf[2] = 0x00;
    buf[3] = 0x01;
    memcpy(buf + 4, &addr->sin_addr.s_addr, 4);
    memcpy(buf + 8, &addr->sin_port, 2);
    if (writen(sock, buf, 10) < 0) {
        LOG_SYS_ERROR;
        return -1;
    }

    if (readn(sock, buf, 10) < 0) {
        LOG_SYS_ERROR;
        return -1;
    }

    if (buf[1] != 0) {
        LOG_FUNCTION_FAILED;
        return -buf[1];
    }

    memcpy(&addr->sin_addr.s_addr, buf + 4, 4); /*get the BND.ADDR from reply message*/
    memcpy(&addr->sin_port, buf + 8, 2);

    return 0;
}

int socks_connect(int sock, struct sockaddr* addr, socklen_t addrlen) /*addr is the address of DST.ADDR*/
{
    if (!addr || addr->sa_family != AF_INET) {
        LOG_FUNCTION_FAILED;
        return -1;
    }
    if ((sock = socks_establish(sock)) < 0) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    if (socks_cmd(sock, 0x01, (struct sockaddr_in*)addr) < 0) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    return sock;
}

int socks_listen(int sock, struct sockaddr* addr, socklen_t addrlen) /*processing BIND operation*/
{
    if (!addr || addr->sa_family != AF_INET) {
        LOG_FUNCTION_FAILED;
        return -1;
    }
    if ((sock = socks_establish(sock)) < 0) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    if (socks_cmd(sock, 0x02, (struct sockaddr_in*)addr) < 0) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    return sock;
}

int socks_accept(int sock, struct sockaddr* from, socklen_t* fromlen) /*get ADDR and PORT of remote connecting host from the 
									  second reply*/
{
    unsigned char buf[10];

    if (readn(sock, buf, 10) < 0) {
        LOG_SYS_ERROR;
        return -1;
    }

    if (buf[1] != 0) {
        DPRINT("%s(): sock=%d buf[]={%d(%c), %d(%c)\n", __FUNCTION__, sock, buf[0], buf[0], buf[1], buf[1]);
        LOG_FUNCTION_FAILED;
        return -buf[1];
    }

    if (*fromlen < sizeof(struct sockaddr_in)) {
        LOG_FUNCTION_FAILED;
        return -1;
    } else {
        struct sockaddr_in* addr = (struct sockaddr_in*)from;

        addr->sin_family = AF_INET;
        memcpy(&addr->sin_addr.s_addr, buf + 4, 4);
        memcpy(&addr->sin_port, buf + 8, 2);
    }

    //return dup(sock);
    return sock;
}

int socks_associate(int sock, struct sockaddr* addr, socklen_t addrlen)
{
    DPRINT("%s(): %s\n", __FUNCTION__, sock_ntop_host(addr, addrlen));
    if (!addr || addr->sa_family != AF_INET) {
        LOG_FUNCTION_FAILED;
        return -1;
    }
    if ((sock = socks_establish(sock)) < 0) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    if (socks_cmd(sock, 0x03, (struct sockaddr_in*)addr) < 0) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    return sock;
}

int socks_initiate(int sock, struct sockaddr* relay_addr, socklen_t addrlen)
{
    int count = 8;

    while (count--) {
        int res;
        unsigned char buf[16];
        struct sockaddr_storage fromaddr;
        socklen_t fromlen = sizeof(fromaddr);

        if (socks_keep_alive(sock, relay_addr, addrlen) < 0) {
            continue;
        }

        if (wait_socket(sock, SOCKIN, 250) != SOCKIN) {
            continue;
        }

        if ((res = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&fromaddr, &fromlen)) < 0) {
            LOG_SYS_ERROR;
            continue;
        }

        if (res < 10) {
            DPRINT("%s(): invalid packet!\n", __FUNCTION__);
            continue;
        }

        /*		
		{
			int i;
			printf("%s(): recvfrom %s: ", sock_ntop_host((struct sockaddr*)&fromaddr, fromlen));
			for(i=0; i<res; i++)
				printf("%.02x ", buf[i]);
			printf("\n");
		}
*/
        if (buf[0] != 0xDB || buf[1] != 1) {
            DPRINT("%s(): unexpected packet!\n", __FUNCTION__);
            continue;
        }

        return 0;
    }

    return -1;
}

int socks_keep_alive(int sock, struct sockaddr* relay_addr, socklen_t addrlen)
{
    char buf[10];

    bzero(buf, sizeof(buf));
    buf[0] = 0xDB;
    buf[1] = 0;
    buf[3] = 1;

    if (sendto(sock, buf, sizeof(buf), 0, relay_addr, addrlen) < 0) {
        LOG_SYS_ERROR;
        return -1;
    }

    return 0;
}

int socks_keep_alive_ack(int sock, struct sockaddr* relay_addr, socklen_t addrlen)
{
    char buf[10];

    bzero(buf, sizeof(buf));
    buf[0] = 0xDB;
    buf[1] = 1;
    buf[3] = 1;

    if (sendto(sock, buf, sizeof(buf), 0, relay_addr, addrlen) < 0) {
        LOG_SYS_ERROR;
        return -1;
    }

    return 0;
}
