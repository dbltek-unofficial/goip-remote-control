#ifndef _APP_H
#define _APP_H

#include <sys/poll.h>

#define IPP_0_4_5

#include "timer.h"

#define ARG_NONE 0x00
#define ARG_STRING 0x01
#define ARG_INT 0x02
#define ARG_MULTI 0x80
#define ARG_CFGFILE 0x7F

struct app_option {
    const char* long_name;
    char short_name;
    const char* equiv_env;
    int arg_type;
    const char* descripton;
};

#define APP_GETOPT_INDEX 1
#define APP_GETOPT_COUNT 2

const char* app_getopt(const char* optname, int flags, ...);
int app_opt_isset(const char* optname);

int app_init(int argc, char* argv[], const struct app_option* options, const char* appname);

int app_set_loop_type(int type, void* arg);

int app_run();
void app_quit(int code);

#ifdef _NO_POLL

#define POLLIN 0x0001 /* There is data to read */
#define POLLPRI 0x0002 /* There is urgent data to read */
#define POLLOUT 0x0004 /* Writing now will not block */
#define POLLERR 0x0008 /* Error condition */
#define POLLHUP 0x0010 /* Hung up */
#define POLLNVAL 0x0020 /* Invalid request: fd not open */

#endif

#define SOCKIN 0x1
#define SOCKOUT 0x4
#define SOCKERR 0x8

int register_sock(int sock, unsigned events, void* inst, void* call_back);
void unregister_sock(int sock, void* inst, void* call_back);

#endif
