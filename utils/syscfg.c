#ifndef __MLIBC__
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "debug.h"
#include "syscfg.h"

/* syscfg protocol :
 *
 * 1. set varialbe
 * <varialbe>=<value>		; set <variable> to <value>
 * !OK				; on success
 * !ERR: <error info>		; on error
 * 
 * 2. query variable
 * ?<variale>			; query <varialbe>'s value
 * ><value>			; on success
 * !ERR: <error info>		; on error
 *
 * 3. commands
 * %list			; list all variables
 * ><variable>=<value>
 * ><variable>=<value>
 * ...
 * !OK				; on success
 * !ERR: <error info>		; on error
 * 
 * %del <variable>		; delete varialbe
 * !OK				; on success
 * !ERR:			; on error
 *
 * %save			; save syscfg to flash
 * !OK				; on success
 * !ERR: <error info>		; on error
 *
 * %reload			; reload syscfg from flash
 * !OK				; on success
 * !ERR: <error info>		; on error
 *
 */

#define SYSCFG_PATH "/tmp/.syscfg-server"

static int client_sock = -1;
static char buf[512];

#ifndef __TEST__
static const struct sockaddr_un syscfg_svr_addr = {
#else
static struct sockaddr_un syscfg_svr_addr = {
#endif
    sun_family : AF_UNIX,
    sun_path : { SYSCFG_PATH }
};

static int unix_bind(const char* path)
{
    struct sockaddr_un addr;
    int sock;

    if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "unix_bind(): %s\n", strerror(errno));
        return -1;
    }

    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (path[0] == '\0') {
        memcpy(addr.sun_path + 1, path + 1, strlen(path + 1));
    } else {
        unlink(path);
        strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    }

    if (bind(sock, (struct sockaddr*)&addr, SUN_LEN(&addr)) != 0) {
        close(sock);
        return -1;
    }

    return sock;
}

static int syscfg_p_write(char* buf, int bufsiz)
{
    int res;

    if (client_sock < 0) {
        char sn[30];

        bzero(sn, sizeof(sn));
        sprintf(sn + 1, ".syscfg-client-%d", getpid());
        if ((client_sock = unix_bind(sn)) < 0) {
            unlink(buf);
            return -1;
        }

#ifdef __TEST__
        sprintf(syscfg_svr_addr.sun_path, "%s%s",
            getenv("HOME") ? getenv("HOME") : "",
            SYSCFG_PATH);
        DLOG("syscfg server address: %s", syscfg_svr_addr.sun_path);
#endif
    }

    if ((res = sendto(client_sock, buf, bufsiz, MSG_WAITALL,
             (struct sockaddr*)&syscfg_svr_addr, sizeof(syscfg_svr_addr)))
        <= 0) {
        //fprintf(stderr, "%s(): sendto(): %s: %m\n" , __FUNCTION__, ntop_unix(&syscfg_svr_addr));
        LOG_SYS_ERROR;
        close(client_sock);
        client_sock = -1;
        return -1;
    }

    return res;
}

static int syscfg_p_read(char* buf, int bufsiz)
{
    int res;
    struct sockaddr_un addr;
    socklen_t addrlen = sizeof(addr);

    if (client_sock < 0) {
        return -1;
    }

    bzero(buf, bufsiz);
    if ((res = recvfrom(client_sock, buf, bufsiz, MSG_WAITALL, (struct sockaddr*)&addr, &addrlen)) < 0) {
        //fprintf(stderr, "%s(): recvfrom(): %s: %m\n", __FUNCTION__, ntop_unix(&addr));
        close(client_sock);
        client_sock = -1;
        return 0;
    }

    return res;
}

char* getsyscfg(const char* var)
{
    int len;

    bzero(buf, sizeof(buf));
    len = snprintf(buf, 34, "?%s", var);
    if (syscfg_p_write(buf, len) < 0) {
        return getenv(var);
    }

    if ((len = syscfg_p_read(buf, sizeof(buf) - 1)) < 0) {
        return 0;
    }

    if (buf[0] == '>') {
        buf[len] = 0;
    } else {
        return 0;
    }

    return buf + 1;
}

int setsyscfg(const char* var, const char* value)
{
    int len;

    len = snprintf(buf, sizeof(buf), "%s=%s", var, value);
    if (syscfg_p_write(buf, len) < 0) {
        return setenv(var, value, 1);
    }

    syscfg_p_read(buf, sizeof(buf));
    if (strncmp(buf, "!OK", 3)) {
        return -1;
    }

    return 0;
}

int syscfg_enum(int (*handler)(void*, const char*, const char*), void* data)
{
    char *var, *value, *p;
    int len, skip = 0;

    if (syscfg_p_write("\%list", 5) < 0) {
        return -1;
    }

    while (1) {
        if ((len = syscfg_p_read(buf, sizeof(buf) - 1)) < 0) {
            return -1;
        }
        buf[len] = 0;

        if (!strcmp("!OK", buf))
            break;

        if (buf[0] == '>') {
            if (skip)
                continue;

            var = buf + 1;
            if ((p = strchr(var, '=')) == NULL) {
                return -1;
            }
            *p++ = 0;
            if (*p == '\"') {
                p++;
                value = p;
                if ((p = strchr(value, '\"'))) {
                    *p = '\0';
                }
            } else {
                value = p;
            }

            if (handler(data, var, value)) {
                skip = 1;
            }
        }
    }

    return 0;
}

int syscfg_apply()
{
    int len;

    len = sprintf(buf, "%%apply");
    if (syscfg_p_write(buf, len) < 0) {
        fprintf(stderr, "write failed!\n");
        return -1;
    }

    syscfg_p_read(buf, sizeof(buf));
    if (strncmp(buf, "!OK", 3)) {
        return -1;
    }

    return 0;
}

int syscfg_save()
{
    int len;

    len = sprintf(buf, "%%save");
    if (syscfg_p_write(buf, len) < 0) {
        fprintf(stderr, "write failed!\n");
        return -1;
    }

    syscfg_p_read(buf, sizeof(buf));
    if (strncmp(buf, "!OK", 3)) {
        return -1;
    }

    return 0;
}

int syscfg_reload()
{
    int len;

    len = sprintf(buf, "%%reload");
    if (syscfg_p_write(buf, len) < 0) {
        fprintf(stderr, "write failed!\n");
        return -1;
    }

    syscfg_p_read(buf, sizeof(buf));
    if (strncmp(buf, "!OK", 3)) {
        return -1;
    }

    return 0;
}

#endif
