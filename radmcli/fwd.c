#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "app.h"
#include "fwd.h"
#include "radmc.h"

extern struct radmcs* radmcs;
#define CHECKPOINT fprintf(stderr, "%s():%d: CHECK POINT\n", __FUNCTION__, __LINE__)
#define RESET_TIMER
//#define RESET_TIMER {stop_timer(fwd->radmc->timer);start_timer(fwd->radmc->timer,-1);}

struct fwd {
    struct radmc* radmc;
    struct rc4_state* rc4_dec;
    struct rc4_state* rc4_enc;
    int tsock;
    char tbuf[512];
    int tbuflen;
    int tbufpos;

    int psock;
    char pbuf[512];
    int pbuflen;
    int pbufpos;
};

void tsock_read_callback(struct fwd* fwd, unsigned events, int fd);
void tsock_write_callback(struct fwd* fwd, unsigned events, int fd);
void psock_read_callback(struct fwd* fwd, unsigned events, int fd);
void psock_write_callback(struct fwd* fwd, unsigned events, int fd);

void fwd_release(struct fwd* fwd)
{
    unregister_sock(fwd->tsock, fwd, tsock_read_callback);
    unregister_sock(fwd->tsock, fwd, tsock_write_callback);
    unregister_sock(fwd->psock, fwd, psock_read_callback);
    unregister_sock(fwd->psock, fwd, psock_write_callback);
    if (fwd->rc4_dec)
        free(fwd->rc4_dec);
    if (fwd->rc4_enc)
        free(fwd->rc4_enc);
    close(fwd->tsock);
    close(fwd->psock);
    free(fwd);
}

void psock_write_callback(struct fwd* fwd, unsigned events, int fd)
{
    int res;

    RESET_TIMER
    //CHECKPOINT;
    res = send(fwd->psock, fwd->tbuf + fwd->tbufpos, fwd->tbuflen - fwd->tbufpos, 0);
    if (res < 0) {
        //CHECKPOINT;
        fwd_release(fwd);
        return;
    }
    //fflush(stderr);
    //write(2, fwd->tbuf+fwd->tbufpos, res);

    fwd->tbufpos += res;
    if (fwd->tbufpos == fwd->tbuflen) {
        //CHECKPOINT;
        fwd->tbuflen = 0;
        fwd->tbufpos = 0;
        register_sock(fwd->tsock, SOCKIN, fwd, tsock_read_callback);
        unregister_sock(fwd->psock, fwd, psock_write_callback);
    } else {
        //CHECKPOINT;
    }
}

void tsock_read_callback(struct fwd* fwd, unsigned events, int fd)
{
    int res;

    RESET_TIMER
    //CHECKPOINT;
    //fprintf(stderr, "tsock_read_callback\n");
    res = recv(fwd->tsock, fwd->tbuf, sizeof(fwd->tbuf), 0);
    if (res <= 0) {
        //CHECKPOINT;
        fwd_release(fwd);
        return;
    }

    rc4_crypt(fwd->rc4_dec, fwd->tbuf, res);
    fwd->tbuflen = res;

    res = send(fwd->psock, fwd->tbuf, res, 0);
    if (res < 0) {
        //CHECKPOINT;
        if (errno == EAGAIN) {
            CHECKPOINT;
            res = 0;
        } else {
            //CHECKPOINT;
            fwd_release(fwd);
            return;
        }
    }
    //fflush(stderr);
    //write(2, fwd->tbuf, res);

    //CHECKPOINT;
    fwd->tbufpos = res;
    if (fwd->tbufpos < fwd->tbuflen) {
        //CHECKPOINT;
        unregister_sock(fwd->tsock, fwd, tsock_read_callback);
        register_sock(fwd->psock, SOCKOUT, fwd, psock_write_callback);
    } else {
        //CHECKPOINT;
        fwd->tbuflen = 0;
        fwd->tbufpos = 0;
    }
    //fprintf(stderr, "tsock_read_callback over\n");
}

void tsock_write_callback(struct fwd* fwd, unsigned events, int fd)
{
    int res;

    RESET_TIMER
    //CHECKPOINT;
    //fprintf(stderr, "tsock_write_callback\n");

    res = send(fwd->tsock, fwd->pbuf + fwd->pbufpos, fwd->pbuflen - fwd->pbufpos, 0);
    if (res < 0) {
        //CHECKPOINT;
        fwd_release(fwd);
        return;
    }

    fwd->pbufpos += res;
    if (fwd->pbufpos == fwd->pbuflen) {
        //CHECKPOINT;
        fwd->pbuflen = 0;
        fwd->pbufpos = 0;
        register_sock(fwd->psock, SOCKIN, fwd, psock_read_callback);
        unregister_sock(fwd->tsock, fwd, tsock_write_callback);
    } else {
        //CHECKPOINT;
    }
}

void psock_read_callback(struct fwd* fwd, unsigned events, int fd)
{
    int res;

    RESET_TIMER
    //CHECKPOINT;
    //fprintf(stderr, "psock_read_callback\n");
    res = recv(fwd->psock, fwd->pbuf, sizeof(fwd->pbuf), 0);
    if (res <= 0) {
        //CHECKPOINT;
        fwd_release(fwd);
        return;
    }

    //fflush(stderr);
    //write(2, fwd->pbuf, res);
    rc4_crypt(fwd->rc4_enc, fwd->pbuf, res);
    fwd->pbuflen = res;

    res = send(fwd->tsock, fwd->pbuf, res, MSG_DONTWAIT);
    if (res < 0) {
        //CHECKPOINT;
        if (errno == EAGAIN) {
            //CHECKPOINT;
            res = 0;
        } else {
            //CHECKPOINT;
            fwd_release(fwd);
            return;
        }
    }

    fwd->pbufpos = res;
    if (fwd->pbufpos < fwd->pbuflen) {
        //CHECKPOINT;
        unregister_sock(fwd->psock, fwd, psock_read_callback);
        register_sock(fwd->tsock, SOCKOUT, fwd, tsock_write_callback);
    } else {
        //CHECKPOINT;
        fwd->pbuflen = 0;
        fwd->pbufpos = 0;
    }
}

void do_forward(struct radmc* radmc, int tsock, int psock, struct rc4_state* enc, struct rc4_state* dec)
{
    struct fwd* fwd;

    fprintf(stderr, "do_forwrd: tsock=%d psock=%d\n", tsock, psock);
    fwd = malloc(sizeof(struct fwd));
    memset(fwd, 0, sizeof(struct fwd));

    fwd->tsock = tsock;
    fwd->psock = psock;
    fwd->rc4_enc = enc;
    fwd->rc4_dec = dec;
    fwd->radmc = radmc;

    register_sock(tsock, SOCKIN, fwd, tsock_read_callback);
    register_sock(psock, SOCKIN, fwd, psock_read_callback);
}
