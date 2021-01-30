#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "stun.h"
#include "stunlib.h"
//#include "net.h"

#define DEBUG
#define PLOG fprintf(stderr, "%s:%s():%d : %s\n", __FILE__, __FUNCTION__, __LINE__, strerror(errno))

inline void printaddr(struct sockaddr_in* addr);

void build_shsMsg(StunMessage* msg, const StunAtrString* username);

int sendmessage(int fd, char* buf, int buflen, struct sockaddr_in* servaddr);

void set_udp_socket(stun_handler_t* handler, int udp)
{
    handler->udp_socket = udp;
}

void set_tcp_socket(stun_handler_t* handler, int tcp)
{
    handler->tcp_socket = tcp;
}

void set_nat_num(stun_handler_t* handler, int nat)
{
    handler->nat_num = nat;
}

void stun_getNatAddress(const stun_handler_t* handler, struct sockaddr* nataddr, size_t* len)
{
    if (*len != sizeof(struct sockaddr)) {
        *len = sizeof(struct sockaddr);
    }
    memcpy(nataddr, &handler->nataddr, *len);
}

void stun_trysend(int fd, int num, const struct sockaddr* addr, size_t len, int repeat)
{
    int i, ttl = 255;
    int nat = num + 1;
    char* msg = "ha";
    size_t addrlen = sizeof(struct sockaddr_in);

    if (addr->sa_data == INADDR_ANY) {
        return;
    }
    if (setsockopt(fd, IPPROTO_IP, IP_TTL, &nat, sizeof(nat)) != 0) {
        PLOG;
        return;
    }

    for (i = 0; i < repeat; i++) {
        fprintf(stderr, " send try(ttl=%d) to %s:%d \n", nat,
            inet_ntoa(((struct sockaddr_in*)addr)->sin_addr),
            ntohs(((struct sockaddr_in*)addr)->sin_port));
        if (sendto(fd, msg, strlen(msg), 0, addr, sizeof(*addr)) < 0) {
            PLOG;
            setsockopt(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
            return;
        }
    }
    if (setsockopt(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) != 0) {
        PLOG;
        return;
    }
}

#if 0
int stun_bind(int fd)
{
	struct sockaddr_in cliaddr;
	int err;

	bzero(&cliaddr,sizeof(cliaddr));
	cliaddr.sin_family=AF_INET;
	cliaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	cliaddr.sin_port=htons(0);
	
	if( bind(fd,(struct sockaddr *)&cliaddr,sizeof(cliaddr)) != 0){
		err=errno;
		switch(err){
	  	case 0:
			PLOG;
			//fprintf(stderr,"Could not bind socket: %s\n",strerror(err));
			return -1;
		case EADDRINUSE:
			PLOG;
			//fprintf(stderr,"Port %d for receiving UDP is in use \n", ntohs(cliaddr.sin_port));
			return -1;
		case EADDRNOTAVAIL:
		   PLOG;
			//fprintf(stderr,"Cannot assign requested address:%s\n",strerror(err));
			return -1;
		default:
			PLOG;
			//fprintf(stderr, "Could not bind UDP recevive port : %s \n", strerror(err));
			return -1;
		}
	}
	return 0;
}
#endif

void stun_handler_init(stun_handler_t* handler)
{
    handler->udp_socket = -1;
    handler->tcp_socket = -1;
    handler->nat_num = 0;
    memset(&handler->servaddr, 0, sizeof(struct sockaddr_in));
    memset(&handler->nataddr, 0, sizeof(struct sockaddr_in));
    memset(handler->tmpusername.value, 0, sizeof(handler->tmpusername.value));
    memset(handler->tmppassword.value, 0, sizeof(handler->tmppassword.value));
    memset(handler->originalusername.value, 0, sizeof(handler->originalusername.value));
    memset(handler->originalpassword.value, 0, sizeof(handler->originalpassword.value));

    handler->tmpusername.sizeValue = 0;
    handler->tmppassword.sizeValue = 0;
    handler->originalusername.sizeValue = 0;
    handler->originalpassword.sizeValue = 0;
}

void set_stun_servaddr(stun_handler_t* handler, const struct sockaddr_in* servaddr, size_t* len)
{
    if (*len != sizeof(struct sockaddr_in)) {
        *len = sizeof(struct sockaddr_in);
    }
    memcpy(&handler->servaddr, servaddr, *len);
    //	fprintf(stderr,"servaddr= %s port: %d \n",inet_ntoa(handler->servaddr.sin_addr),
    //			ntohs(handler->servaddr.sin_port));
}

int send_bindingreq(stun_handler_t* handler)
{
    StunMessage msg;
    char buf[STUN_MAX_MESSAGE_SIZE];
    int buflen = sizeof(buf);
    int len, s;
    struct sockaddr_in cliaddr;

    bzero(&cliaddr, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    cliaddr.sin_port = htons(0);

    //	fprintf(stderr,"servaddr= %s port: %d \n",inet_ntoa(handler->servaddr.sin_addr),
    //			ntohs(handler->servaddr.sin_port));

    if (handler->udp_socket == -1) {
        handler->udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (handler->udp_socket == -1) {
            PLOG;
            //fprintf(stderr,"Could not create a UDP socket in send_bindingreq()\n");
            return -1;
        }
        if (bind(handler->udp_socket, (struct sockaddr*)&cliaddr, sizeof(cliaddr)) != 0) {
            PLOG;
            return -1;
        }
    }

    stunBuildReqSimple(&msg, &handler->tmpusername, 0, 0, 0);
#ifdef DEBUG
    len = stunEncodeMessage(&msg, buf, buflen, &handler->tmppassword, 1);
    printf("About to send msg of len %d to ", len);
    printaddr(&handler->servaddr);
    s = sendmessage(handler->udp_socket, buf, len, &handler->servaddr);
#else
    len = stunEncodeMessage(&msg, buf, buflen, &handler->tmppassword, 0);
    s = sendmessage(handler->udp_socket, buf, len, &handler->servaddr);
#endif
    return s;
}

inline void printaddr(struct sockaddr_in* addr)
{
    printf("%s \n", inet_ntoa(addr->sin_addr));
}

int send_shsreq(stun_handler_t* handler)
{
    StunMessage msg;
    char buf[STUN_MAX_MESSAGE_SIZE];
    int buflen = sizeof(buf);
    int len, s, err;

    if (handler->tcp_socket == -1) {
        handler->tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (handler->tcp_socket == -1) {
            PLOG;
            //fprintf(stderr,"%s line:%d :%s\n",__FUNCTION__,__LINE__,strerror(error));
            return -1;
        }
    }
    if (connect(handler->tcp_socket, (struct sockaddr*)&handler->servaddr, sizeof(handler->servaddr)) != 0) {
        err = errno;
        switch (err) {
        case EISCONN:
            PLOG;
            //fprintf(stderr,"The socket is already connected \n");
            return -1;
        case ECONNREFUSED:
            PLOG;
            //fprintf(stderr," No one listening on the remote address \n");
            return -1;
        case ENETUNREACH:
            PLOG;
            //fprintf(stderr,"Network is unreachable \n");
            return -1;
        default:
            PLOG;
            //fprintf(stderr,"some unknow reason not connect remote server \n");
            return -1;
        }
    }
    build_shsMsg(&msg, &handler->originalusername);
#ifdef DEBUG
    len = stunEncodeMessage(&msg, buf, buflen, &handler->originalpassword, 1);
    printf("About to send msg of len %d to ", len);
    printaddr(&handler->servaddr);
    sendmessage(handler->tcp_socket, buf, len, &handler->servaddr);
#else
    len = stunEncodeMessage(&msg, buf, buflen, &handler->originalpassword, 0);
    s = sendmessage(handler->tcp_socket, buf, len, &handler->servaddr);
#endif
    return s;
}

void build_shsMsg(StunMessage* msg, const StunAtrString* username)
{
    int r, i;
    memset(msg, 0, sizeof(*msg));

    msg->msgHdr.msgType = SharedSecretRequestMsg;
    for (i = 0; i < 16; i = i + 4) {
        r = random();
        msg->msgHdr.id.octet[i + 0] = r >> 0;
        msg->msgHdr.id.octet[i + 1] = r >> 8;
        msg->msgHdr.id.octet[i + 2] = r >> 16;
        msg->msgHdr.id.octet[i + 3] = r >> 24;
    }
    if (username->sizeValue > 0) {
        msg->hasUsername = 1;
        msg->username = *username;
    }
}

int sendmessage(int fd, char* buf, int buflen, struct sockaddr_in* servaddr)
{
    int nleft, err, nsend;
    const char* ptr;
    socklen_t tolen = sizeof(struct sockaddr_in);

    ptr = buf;
    nleft = buflen;
    while (nleft > 0) {
        if ((nsend = sendto(fd, ptr, nleft, 0, (struct sockaddr*)servaddr, tolen)) < 0) {
            if (nsend == -1) {
                err = errno;
                switch (err) {
                case ECONNREFUSED: {
                    PLOG;
                    //fprintf(stderr,"connect refuseed in sendmessage():%s\n",strerror(err));
                    return -1;
                }

                case EHOSTDOWN: {
                    PLOG;
                    //fprintf(stderr,"EHOSTDOWN in sendmessage():%s\n",strerror(err));
                    return -1;
                }
                case EHOSTUNREACH: {
                    PLOG;
                    //fprintf(stderr,"host unreach in sendmessage():%s\n",strerror(err));
                    return -1;
                }
                case EAFNOSUPPORT: {
                    PLOG;
                    //fprintf(stderr, "err EAFNOSUPPORT in sendmessage():%s\n",strerror(err));
                    return -1;
                }
                default: {
                    PLOG;
                    //fprintf(stderr, "error in sendmessage():%s\n", strerror(err));
                    return -1;
                }
                }
            }
        } //if
        nleft -= nsend;
        ptr += nsend;
    } //while
    return buflen;
}

int recv_bindingresp(stun_handler_t* handler)
{
    struct sockaddr_in from;
    size_t fromlen = sizeof(from);
    int msglen;
    char buf[STUN_MAX_MESSAGE_SIZE];
    int buflen = sizeof(buf);
    StunMessage resp;

    if (handler->udp_socket == -1) {
        PLOG;
        //printf(" invaild UDP socket \n");
        return -1;
    }
    msglen = recvfrom(handler->udp_socket, buf, buflen, 0, (struct sockaddr*)&from, &fromlen);
    if (msglen < 0 && msglen == 0) {
        PLOG;
        //printf("socket closed ? \n");
        return -1;
    }
    if (msglen + 1 > buflen) {
        PLOG;
        //printf("Received a binding response message that was too large\n");
        return -1;
    }
#ifdef DEBUG
    stunParseMessage(buf, msglen, &resp, 1);
#else
    stunParseMessage(buf, msglen, &resp, 0);
#endif
    if (resp.msgHdr.msgType == BindResponseMsg) {
        handler->nataddr.sin_family = AF_INET;
        handler->nataddr.sin_addr.s_addr = htonl(resp.mappedAddress.ipv4.addr);
        handler->nataddr.sin_port = htons(resp.mappedAddress.ipv4.port);
    } else
        return 0;
    return msglen;
}

int recv_shsresp(stun_handler_t* handler)
{
    struct sockaddr_in from;
    size_t fromlen = sizeof(from);
    int msglen;
    char buf[STUN_MAX_MESSAGE_SIZE];
    int buflen = sizeof(buf);
    StunMessage resp;

    if (handler->tcp_socket == -1) {
        PLOG;
        //printf("invalid TCP	 socket \n");
        return -1;
    }
    msglen = recvfrom(handler->tcp_socket, buf, buflen, 0, (struct sockaddr*)&from, &fromlen);
    if (msglen < 0 && msglen == 0) {
        PLOG;
        //printf("socket closed  \n");
        return -1;
    }
    if (msglen + 1 > buflen) {
        PLOG;
        //printf("Received a shared secret message that was too large\n");
        return -1;
    }
#ifdef DEBUG
    stunParseMessage(buf, msglen, &resp, 1);
#else
    stunParseMessage(buf, msglen, &resp, 1);
#endif
    if (resp.msgHdr.msgType == SharedSecretResponseMsg) {
        memcpy(&handler->tmpusername, &resp.username, STUN_MAX_STRING);
        memcpy(&handler->tmppassword, &resp.password, STUN_MAX_STRING);
    } else
        return 0;
    return msglen;
}
