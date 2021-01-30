#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app.h"
#include "debug.h"
#include "fwd.h"
#include "radmc.h"
#include "timer.h"

#define CLOSE(sock)      \
    {                    \
        if (sock > 0) {  \
            close(sock); \
            sock = -1;   \
        }                \
    }

#define FREE(sth)       \
    {                   \
        if (sth) {      \
            free(sth);  \
            sth = NULL; \
        }               \
    }
struct radmcs* radmcs;

void radmc_callback(struct radmc* radmc, unsigned events, int fd);
void radmc_timeout_callback(struct radmc* radmc);

void radmc_reset(struct radmc* radmc)
{
    FREE(radmc->rc4_enc);
    unregister_sock(radmc->sock, radmc, radmc_callback);
    CLOSE(radmc->sock);
}

void radmc_release(struct radmc* radmc)
{
    if (!radmc)
        return;
    fprintf(stderr, "radmc_release\n");
    radmc_reset(radmc);
    //free(radmc->rc4_key);
    free(radmc->id);
    //unregister_sock(radmc->sock, radmc, radmc_callback);
    //CLOSE(radmc->sock);
    if (radmc->timer)
        destroy_timer(radmc->timer);
    //if(radmc->rc4_enc)free(radmc->rc4_enc);
    free(radmc);
    if (radmcs->radmc_http == radmc)
        radmcs->radmc_http = NULL;
    if (radmcs->radmc_telnet == radmc)
        radmcs->radmc_telnet = NULL;
    if (radmcs->radmc_http == NULL && radmcs->radmc_telnet == NULL)
        exit(-1);
    //exit(-1);
}

int radmc_reconnect(struct radmc* radmc);
/*
void radmc_restart(struct radmc *radmc)
{
	int type, timeout;
	type=radmc->IS_RADMIN;
	timeout=radmc->timer->interval;
	radmc_release(radmc);
	if(type == RADMIN) radmcs->radmc_http=
}
*/
void radmc_callback(struct radmc* radmc, unsigned events, int fd)
{
    int sock, n;
    struct rc4_state* rc4_dec;
    char buf[1];

    DLOG("data com");

    if ((events & SOCKERR) || (n = recv(radmc->sock, buf, sizeof(buf), MSG_PEEK)) < 0) {
        DLOG("recv error:%m");
        radmc_reset(radmc);
        //radmc_reconnect(radmc);
        goto TRY_AGAIN;
        //radmc_release(radmc);
        return;
    } else if (n == 0) {
        DLOG("recv error:%m");
        radmc_reset(radmc);
        goto TRY_AGAIN; //20170302 new
        //unregister_sock(radmc->sock, radmc, radmc_callback);
        return;
    }
    //fprintf(stderr, "radmc callback\n");

    //	stop_timer(radmcs->timer);
    //	start_timer(radmcs->timer, 1);
    //	if(radmc->timer){
    stop_timer(radmc->timer);
    start_timer(radmc->timer, -1);
    //	}
    DLOG("data come, connect to local addr");
    unregister_sock(radmc->sock, radmc, radmc_callback);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        DLOG("cannot create socket: %m\n");
        radmc_reset(radmc);
        //unregister_sock(radmc->sock, radmc, radmc_callback);
        //CLOSE(radmc->sock);
        //if(radmc->IS_RADMIN == RADMIN)
        goto TRY_AGAIN;
        //radmc_release(radmc);
        return;
    }

    if (connect(sock, (struct sockaddr*)&radmc->laddr, sizeof(radmc->laddr)) < 0) { //local?
        DLOG("cannot connect to %s:%d: %m\n", inet_ntoa(radmc->laddr.sin_addr), ntohs(radmc->laddr.sin_port));
        CLOSE(sock);
        radmc_reset(radmc);
        //unregister_sock(radmc->sock, radmc, radmc_callback);
        //CLOSE(radmc->sock);
        goto TRY_AGAIN;
        //radmc_release(radmc);
        return;
    }

    rc4_dec = malloc(sizeof(struct rc4_state));
    rc4_setup(rc4_dec, radmc->rc4_key, radmc->rc4_keylen);

    fprintf(stderr, "do forward\n");
    do_forward(radmc, radmc->sock, sock, radmc->rc4_enc, rc4_dec);
TRY_AGAIN:
    //CHECKPOINT;
    if (radmc_reconnect(radmc) < 0) {
        CHECKPOINT;
        //start_timer(radmc->timer, -1);
        radmc_release(radmc);
        return;
    }
}

int radmc_reconnect(struct radmc* radmc)
{
    int sock, res;
    char buf[256];

    //fprintf(stderr, "new connection\n");
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        DLOG(stderr, "cannot create socket: %m\n");
        sleep(10);
        exit(-1);
    }

    if (connect(sock, (struct sockaddr*)&radmc->raddr, sizeof(radmc->raddr)) < 0) {
        close(sock);
        DLOG("cannot connect to %s:%d: %m\n", inet_ntoa(radmc->raddr.sin_addr), ntohs(radmc->raddr.sin_port));
        sleep(10);
        exit(-1);
        return -1;
    }

    radmc->sock = sock;
    radmc->rc4_enc = malloc(sizeof(struct rc4_state));
    if (radmc->rc4_enc == NULL) {
        CHECKPOINT;
        sleep(10);
        exit(-1);
    }
    rc4_setup(radmc->rc4_enc, radmc->rc4_key, radmc->rc4_keylen);

    if (radmc->IS_RADMIN) {
        res = sprintf(buf, "RADMIN: %s\n", radmc->id);
        DLOG("send RADMIN: %s", radmc->id);
    } else {
        res = sprintf(buf, "TELNET: %s\n", radmc->id);
        DLOG("send RLOGIN: %s", radmc->id);
    }

    rc4_crypt(radmc->rc4_enc, buf, res);
    if (send(radmc->sock, buf, res, 0) < 0) {
        radmc_reset(radmc);
        DLOG("send error: %m\n");
        sleep(10);
        exit(-1);
        return -1;
    }

    register_sock(sock, SOCKIN, radmc, radmc_callback);
    //fprintf(stderr, "id sent, waiting data\n");

    if (radmc->timer) {
        CHECKPOINT;
        stop_timer(radmc->timer);
        start_timer(radmc->timer, -1);
    }
    //fprintf(stderr, "timer start\n");

    return 0;
}

void radmc_keepalive(struct radmc* radmc)
{
    //char buf[64];
    //int res;

    //stop_timer(radmc->timer);

    radmc_timeout_callback(radmc);
    /*
	if(radmc->IS_RADMIN){
		unregister_sock(radmc->sock, radmc, radmc_callback);
		CLOSE(radmc->sock);
		if(radmc_reconnect(radmc)<0){
			CHECKPOINT;
			radmc_release(radmc);
		}
		return;
		//res=sprintf(buf, "RADMIN: %s\n", radmc->id);
		//DLOG("send RADMIN: %s\n", radmc->id);
	}
	else {
		res=sprintf(buf, "TELNET: %s\n", radmc->id);
                DLOG("send RLOGIN: %s\n", radmc->id);
	}

	rc4_crypt(radmc->rc4_enc, buf, res);
	send(radmc->sock, buf, res, MSG_DONTWAIT);

	start_timer(radmc->timer, -1);
*/
}

void radmc_timeout_callback(struct radmc* radmc)
{
    //if(radmc->sock) unregister_sock(radmc->sock, radmc, radmc_callback);
    //CLOSE(radmc->sock);
    CHECKPOINT;
    radmc_reset(radmc);
    if (radmc_reconnect(radmc) < 0) {
        CHECKPOINT;
        //start_timer(radmc->timer, -1);
        //radmc_release(radmc);
    }
}

struct radmc* radmc_create(struct sockaddr_in* raddr, struct sockaddr_in* laddr, char* key, char* id, int IS_RADMIN, int timeout)
{
    struct radmc* radmc;

    radmc = malloc(sizeof(struct radmc));
    memset(radmc, 0, sizeof(struct radmc));

    memcpy(&radmc->raddr, raddr, sizeof(struct sockaddr_in));
    memcpy(&radmc->laddr, laddr, sizeof(struct sockaddr_in));
    radmc->IS_RADMIN = IS_RADMIN;
    radmc->rc4_key = strdup(key);
    radmc->rc4_keylen = strlen(key);
    radmc->id = strdup(id);

    if (timeout > 0)
        radmc->timer = create_timer(timeout * SECOND, radmc, radmc_keepalive);
    else
        radmc->timer = create_timer(30 * SECOND, radmc, radmc_keepalive);
    //start_timer(radmc->timer, -1);

    //	if(IS_RADMIN == RADMIN) radmc->timer=create_timer((timeout>0?timeout:1200)*SECOND, radmc, radmc_timeout_callback);
    if (radmc_reconnect(radmc) < 0) {
        CHECKPOINT;
        radmc_release(radmc);
        return NULL;
    }

    return radmc;
}

struct radmcs* radmcs_create(struct sockaddr_in* raddr, struct sockaddr_in* A_laddr, struct sockaddr_in* L_laddr, char* key, char* id, int timeout)
{
    radmcs = calloc(sizeof(struct radmcs), 1);
    if (A_laddr->sin_family) {
        fprintf(stderr, "RADMIN local addr: %s:%d\n", inet_ntoa(A_laddr->sin_addr), ntohs(A_laddr->sin_port));
        if ((radmcs->radmc_http = radmc_create(raddr, A_laddr, key, id, RADMIN, timeout)) == NULL) {
            fprintf(stderr, "cannot create radmc\n");
            return NULL;
        }
    }

    if (L_laddr->sin_family) {
        fprintf(stderr, "RLOGIN local addr: %s:%d\n", inet_ntoa(L_laddr->sin_addr), ntohs(L_laddr->sin_port));
        if ((radmcs->radmc_telnet = radmc_create(raddr, L_laddr, key, id, RLOGIN, timeout)) == NULL) {
            fprintf(stderr, "cannot create radmc\n");
            return NULL;
        }
    }
#if 0
	if(timeout>0)
		radmcs->timer=create_timer(timeout*SECOND, radmcs, radmcs_keepalive);
	else
		radmcs->timer=create_timer(20*60*SECOND, radmcs, radmcs_keepalive);
	start_timer(radmcs->timer, 1);
	fprintf(stderr, "timer start\n");
#endif
    return radmcs;
}
