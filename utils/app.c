#include <ctype.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <unistd.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>

#include "app.h"
#include "btrace.h"
#include "net.h"
#include "scheduler.h"
#include "syscfg.h"
#include "utils.h"

#include "debug.h"

static int parse_args(int argc, char* argv[], const struct app_option* options);
static void load_config_file(const struct app_option* options, const char* appname);
static void load_syscfg(const struct app_option* options);

static const char* __app_name;

#ifdef _NO_POLL
struct pollfd {
    int fd; /* file descriptor */
    short events; /* requested events */
    short revents; /* returned events */
};

int poll(struct pollfd* pfd_list, int pfd_count, int timeout)
{
    fd_set rdset, wrset, epset;
    struct timeval tv;
    int i, fdmax = 0, count;

    FD_ZERO(&rdset);
    FD_ZERO(&wrset);
    FD_ZERO(&epset);

    for (i = 0; i < pfd_count; i++) {
        if (pfd_list[i].fd > fdmax)
            fdmax = pfd_list[i].fd;
        if (pfd_list[i].events & SOCKIN) {
            FD_SET(pfd_list[i].fd, &rdset);
        }
        if (pfd_list[i].events & SOCKOUT) {
            FD_SET(pfd_list[i].fd, &wrset);
        }
        FD_SET(pfd_list[i].fd, &epset);
    }
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    if ((count = select(fdmax + 1, &rdset, &wrset, &epset, timeout < 0 ? 0 : &tv)) < 0) {
        return count;
    }

    for (i = 0; i < pfd_count; i++) {
        if (FD_ISSET(pfd_list[i].fd, &rdset)) {
            pfd_list[i].revents |= SOCKIN;
        }
        if (FD_ISSET(pfd_list[i].fd, &wrset)) {
            pfd_list[i].revents |= SOCKOUT;
        }
        if (FD_ISSET(pfd_list[i].fd, &epset)) {
            pfd_list[i].revents |= SOCKERR;
        }
    }

    return count;
}

#endif

static struct pollfd* pfd_list = 0;
static struct pfd_call_back {
    void* ctxt;
    void (*call_back)(void*, unsigned, int);
#ifndef NDEBUG
    const char* call_back_name;
#endif
}* pfd_call_backs = 0;
int pfd_size = 0, pfd_count = 0, pfd_defrag = 0;

int find_registered_sock(int sock, void* inst, void* call_back)
{
    int i;

    for (i = 0; i < pfd_count; i++) {
        if (sock == pfd_list[i].fd && inst == pfd_call_backs[i].ctxt && call_back == pfd_call_backs[i].call_back)
            return i;
    }

    return -1;
}

void unregister_object(void* object)
{
    int i;

    for (i = 0; i < pfd_count; i++) {
        if (object == pfd_call_backs[i].ctxt) {
            pfd_list[i].fd = -1;
            pfd_list[i].events = 0;
            pfd_defrag++;
        }
    }
    CANCEL_ALL_SCHEDULE(object);
}

void unregister_sock(int sock, void* inst, void* call_back)
{
    int i;

    if ((i = find_registered_sock(sock, inst, call_back)) >= 0) {
        pfd_list[i].fd = -1;
        pfd_list[i].events = 0;
        pfd_defrag++;
    }
}

int register_sock(int sock, unsigned events, void* inst, void* call_back)
{
    int i;

    if ((i = find_registered_sock(sock, inst, call_back)) >= 0) {
        pfd_list[i].events = events;
        return i;
    }

    if (pfd_count + 1 > pfd_size) {
        struct pollfd* pfds;
        struct pfd_call_back* call_backs;
        int size = pfd_size + 16;

        pfds = (struct pollfd*)realloc(pfd_list,
            sizeof(struct pollfd) * size);

        if (!pfds)
            return -1;
        pfd_list = pfds;

        call_backs = (struct pfd_call_back*)realloc(pfd_call_backs,
            sizeof(struct pfd_call_back) * size);
        if (!call_backs)
            return -1;
        pfd_call_backs = call_backs;

        pfd_size = size;
    }

    pfd_list[pfd_count].fd = sock;
    pfd_list[pfd_count].events = events;
    pfd_call_backs[pfd_count].ctxt = inst;
    pfd_call_backs[pfd_count].call_back = call_back;
#ifndef NDEBUG
    pfd_call_backs[pfd_count].call_back_name = addr_to_name(call_back);
#endif

    pfd_count++;
    return pfd_count;
}

void pfd_defragment()
{
    int i = 0, p = 0;

    for (i = 0; i < pfd_count; i++) {
        if (pfd_list[i].fd == -1) {
            p = i++;
            break;
        }
    }

    for (; i < pfd_count; i++) {
        if (pfd_list[i].fd != -1) {
            pfd_list[p] = pfd_list[i];
            pfd_call_backs[p] = pfd_call_backs[i];
            p++;
        }
    }

    pfd_defrag = 0;
    pfd_count = p;
}

void sigio_handler(int sig)
{
    signal(SIGPIPE, sigio_handler);
    DLOG("SIGPIPE!");
    return;
}

extern void __debug_dump_logged_call_stack();
extern int __btrace_from(void*, void*, void**, int);

static int debug_sock, debug_server_addrlen;
static struct sockaddr* debug_server_addr = NULL;

void send_debug_message(char* msg, int msglen)
{
    if (debug_server_addr) {
        DLOG("%s: sending info to debug server...", __app_name);
        sendto(debug_sock, msg, msglen, 0, debug_server_addr, debug_server_addrlen);
    }
}

#if 0
static void dump_call_stack(void *fp, void *sp, void *cp)
{
	static void *ptr[256];
	static char buf[1024];
	register int n, i, len;


	ptr[0]=cp;
	if((n=__btrace_from(fp, sp, ptr+1, 255))<0){
		return;
	}

	len = sprintf(buf, "Backtrace call stacks:\n");
	for ( i = 0; i < n; i++) {
		len += sprintf(buf+len, "%s: %d: %s:[%p]\n", __app_name, i, addr_to_name(ptr[i]), ptr[i]);
	}

	send_debug_message(buf, len);

	{
		int t_fd = 0;
	
		write(t_fd, buf, len);
		fsync(t_fd);
		close(t_fd);

#ifdef __arm__
		if ((t_fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY)) >= 0) {
			write(t_fd, buf, len);
			close(t_fd);
		}
#endif
	}
}

#ifdef __linux__

#ifdef __arm__
struct _sigctxt {
        unsigned long trap_no;
        unsigned long error_code;
        unsigned long oldmask;
        unsigned long arm_r0;
        unsigned long arm_r1;
        unsigned long arm_r2;
        unsigned long arm_r3;
        unsigned long arm_r4;
        unsigned long arm_r5;
        unsigned long arm_r6;
        unsigned long arm_r7;
        unsigned long arm_r8;
        unsigned long arm_r9;
        unsigned long arm_r10;
        unsigned long arm_fp;
        unsigned long arm_ip;
        unsigned long arm_sp;
        unsigned long arm_lr;
        unsigned long arm_pc;
        unsigned long arm_cpsr;
        unsigned long fault_address;
};
#define SIG_HANDLER_ARGLIST int sig, int r1, int r2, int r3, struct _sigctxt sc
#endif

#ifdef __i386__
struct _sigctxt {
        unsigned short gs, __gsh;
        unsigned short fs, __fsh;
        unsigned short es, __esh;
        unsigned short ds, __dsh;
        unsigned long edi;
        unsigned long esi;
        unsigned long ebp;
        unsigned long esp;
        unsigned long ebx;
        unsigned long edx;
        unsigned long ecx;
        unsigned long eax;
        unsigned long trapno;
        unsigned long err;
        unsigned long eip;
        unsigned short cs, __csh;
        unsigned long eflags;
        unsigned long esp_at_signal;
        unsigned short ss, __ssh;
        struct _fpstate * fpstate;
        unsigned long oldmask;
        unsigned long cr2;
};
#define SIG_HANDLER_ARGLIST int sig, struct _sigctxt sc
#endif

#else
#define SIG_HANDLER_ARGLIST int sig
#endif

void sigtrap_handler(SIG_HANDLER_ARGLIST)
{
	if (sig != SIGTRAP) {
		return;
	}
	DLOG("SIGTRAP caught!!!!!!");

#ifndef NDEBUG
#ifdef __linux__
#ifdef __arm__
	DLOG("fp=%08lx sp=%08lx pc=%08lx", sc.arm_fp, sc.arm_sp, sc.arm_pc);
	dump_call_stack((void*)sc.arm_fp, (void*)sc.arm_sp, (void*)sc.arm_pc);
#endif
#ifdef __i386__
	dump_call_stack((void*)sc.ebp, (void*)sc.esp, (void*)sc.eip);
#endif
#endif
#endif

	if (signal(SIGTRAP, (void(*)(int))sigtrap_handler) == SIG_ERR) {
		printf("signal error: %s\n", strerror(errno));
		exit(1);
	}
	
//	exit(0);

	return ;
}

#ifdef DEBUG_LAUNCH_GDB
static void launch_gdb()
{
	char pid[40];

	if(!getenv("LAUNCH_GDB")){
		return;
	}

#ifdef __arm__
	sprintf(pid, "%d", getpid());

	if(fork()!=0){
		sleep(2);
		return;
	}
	
	if(execlp("gdbserver", "gdbserver", "0.0.0.0:5555", "--attach", pid, NULL)<0){
		exit(-1);
	}
#else
	sprintf(pid, "--pid=%d", getpid());

	if(fork()!=0){
		sleep(2);
		return;
	}
	
	if(execlp("gdb", "gdb", pid, NULL)<0){
		exit(-1);
	}
#endif
}
#endif

void sighandler(SIG_HANDLER_ARGLIST)
{
	static int in_fault_handler=0;
	const char *signame;
	char signoname[32];
	int i;

	if(in_fault_handler){
		raise(SIGQUIT);
		return;
	}
	in_fault_handler=1;
	
	switch(sig){
		case SIGILL:
			signame="Illegal instruction";
			break;
		case SIGSEGV:
			signame="Sigment fault";
			break;
		case SIGABRT:
			signame="Aborted";
			break;
		case SIGTERM:
			signame="Killed by SIGTERM";
		default:
			sprintf(signoname, "Kill by %d", sig);
			signame=signoname;
			break;
			
	}
	DPRINT("%s!\n", signame);

#ifndef NDEBUG
#ifdef __linux__
#ifdef __arm__
	dump_call_stack((void*)sc.arm_fp, (void*)sc.arm_sp, (void*)sc.arm_pc);
#endif
#ifdef __i386__
	dump_call_stack((void*)sc.ebp, (void*)sc.esp, (void*)sc.eip);
#endif
#endif
#ifdef BUG_REPORT
	bugreport();
#endif
#endif
	flush_log_msg();

#ifdef MEMDEBUG
#ifndef MEMLEAK_DEBUG_ONLY
	DPRINT("__memdebug_check(): ...\n");
	__memdebug_check();
	DPRINT("done\n");
#endif
#endif	

	for(i = 0; i < NOFILE; ++i){
		fsync(i);
		close(i);
	}

#ifdef DEBUG_LAUNCH_GDB
	launch_gdb();
#endif

	exit(0x80 | sig);
}

#endif

void sigquit_handler(int sig)
{
    app_quit(0);
}

void setup_sighandler(void)
{
#if 0
#ifdef USE_SIGSTACK
#ifndef SIGSTSZ
#define SIGSTSZ 4096
#endif
	struct sigaction sa;
	stack_t ss;
	static char sigstack[SIGSTSZ];

	ss.ss_sp = sigstack + sizeof(sigstack);
	ss.ss_size = sizeof(sigstack);
	ss.ss_flags = SS_DISABLE;
	sigaltstack(&ss, NULL);

	sa.sa_handler = (void(*)(int))sighandler; 
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_ONSTACK;
	
	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGILL, &sa, NULL);
#else
	signal(SIGSEGV, (void(*)(int))sighandler);
	signal(SIGILL, (void(*)(int))sighandler);
#endif
	signal(SIGPIPE,(void(*)(int)) sigio_handler);
	signal(SIGABRT, (void(*)(int))sighandler);
	signal(SIGQUIT, sigquit_handler);
	signal(SIGTRAP, (void(*)(int))sigtrap_handler);
#else
    signal_handler_init();
    signal(SIGQUIT, sigquit_handler);
#endif
}

struct scheduler __app_thread_scheduler;

static void init_random_seed()
{
    int fd;
    long seed;

    if ((fd = open("/dev/urandom", O_RDONLY | O_NONBLOCK)) >= 0) {
        read(fd, &seed, sizeof(seed));
        close(fd);
    } else {
        struct timeval tv;
        gettimeofday(&tv, 0);
        seed = tv.tv_usec;
    }

    srand(seed);
    srandom(seed);
}

static void app_atexit()
{
    int i;

#ifdef MEMDEBUG
    /* check memory leak */
    __memdebug_print();
#endif
    //DLOG("exiting ...");

    flush_log_msg();

    for (i = 0; i < NOFILE; ++i) {
        fsync(i);
        close(i);
    }
}

int app_init(int argc, char* argv[], const struct app_option* options, const char* appname)
{
    __app_name = appname;

    if (options) {
        if (parse_args(argc, argv, options) < 0) {
            exit(-1);
        }
        load_config_file(options, appname);
        load_syscfg(options);
    }

    init_random_seed();

#ifndef NDEBUG
    btrace_init();

    if (getenv("DEBUG_SERVER")) {
        struct sockaddr_storage addr;
        socklen_t addrlen = sizeof(addr);

        if (sock_aton(getenv("DEBUG_SERVER"), (struct sockaddr*)&addr, &addrlen) == 0) {
            int on = 1;

            debug_server_addr = malloc(addrlen);
            memcpy(debug_server_addr, &addr, addrlen);
            debug_server_addrlen = addrlen;

            debug_sock = socket(debug_server_addr->sa_family, SOCK_DGRAM, 0);
            setsockopt(debug_sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
        }
    }

#endif
    scheduler_init(&__app_thread_scheduler);

    setup_sighandler();

    atexit(app_atexit);

    return 0;
}

static int quit_flag = 0;

extern int timer_check_timeout();

static void dispatch_sock_events(int nevents)
{
    int i;

    for (i = 0; i < pfd_count && nevents; i++) {
        if (pfd_list[i].revents && pfd_list[i].fd >= 0 // in case that the socket was unregistered by previous callbacks
        ) {
            unsigned events = 0;

            if ((pfd_list[i].revents & POLLIN) || (pfd_list[i].revents & POLLPRI)) {
                events |= SOCKIN;
            }
            if ((pfd_list[i].revents & POLLOUT)) {
                events |= SOCKOUT;
            }
            if (pfd_list[i].revents & POLLERR) {
                DLOG("Error: %d callback=%s:[%p]", pfd_list[i].fd,
                    addr_to_name(pfd_call_backs[i].call_back),
                    pfd_call_backs[i].call_back);
                events |= SOCKERR;
            }
            if (pfd_list[i].revents & POLLNVAL) {
                DLOG("Invalid: %d callback=%s:[%p]", pfd_list[i].fd,
                    addr_to_name(pfd_call_backs[i].call_back),
                    pfd_call_backs[i].call_back);
                events |= SOCKERR;
            }
            if (pfd_list[i].revents & POLLHUP) {
                DLOG("Hang up: %d callback=%s:[%p]", pfd_list[i].fd,
                    addr_to_name(pfd_call_backs[i].call_back),
                    pfd_call_backs[i].call_back);
                events |= SOCKERR;
            }
            //printf(__FUNCTION__"(): i=%d fd=%d events=%d revents=%d", i, pfd_list[i].fd, pfd_list[i].events, pfd_list[i].revents);
            if (events)
                CALL(pfd_call_backs[i].call_back, (pfd_call_backs[i].ctxt, events, pfd_list[i].fd));
            pfd_list[i].revents = 0;
            nevents--;
        }
    }
}

static int app_loop_type = 0;
static int app_loop_inval = 20000;

int app_set_loop_type(int type, void* arg)
{
    switch (type) {
    case 0:
        app_loop_type = 0;
        break;
    case 1:
        app_loop_type = 1;
        if (arg) {
            memcpy(&app_loop_inval, arg, sizeof(int));
        }
        break;
    default:
        return -1;
    }

    return 0;
}

int app_run()
{
    if (app_loop_type == 1) {
        while (!quit_flag) {
            int retval = 1;

            while (retval > 0) {
                if (pfd_defrag)
                    pfd_defragment();
                /* give those pending write events a higher piority than timers */
                if ((retval = poll(pfd_list, pfd_count, 0)) > 0) {
                    dispatch_sock_events(retval);
                }
                if (retval < 0) {
                    if (errno == EINTR || errno == EAGAIN)
                        continue;
                    LOG_SYS_ERROR;
                }
                if (quit_flag)
                    break;
            }

            timer_check_timeout();
            if (quit_flag)
                break;

            if (pfd_defrag)
                pfd_defragment();

            usleep(app_loop_inval * 1000);
        }
    } else {
        while (!quit_flag) {
            int retval, timeout;

            if (pfd_defrag)
                pfd_defragment();
            /* give those pending write events a higher piority than timers */
            if ((retval = poll(pfd_list, pfd_count, 0)) > 0) {
                dispatch_sock_events(retval);
            }
            if (retval < 0) {
                if (errno == EINTR || errno == EAGAIN)
                    continue;
                LOG_SYS_ERROR;
            }
            if (quit_flag)
                break;

            timeout = timer_check_timeout();
            if (quit_flag)
                break;

            if (pfd_defrag)
                pfd_defragment();
            if ((retval = poll(pfd_list, pfd_count, timeout)) > 0) {
                dispatch_sock_events(retval);
            }
            if (retval < 0) {
                if (errno == EINTR || errno == EAGAIN)
                    continue;
                LOG_SYS_ERROR;
            }
        }
    }

    {
        int i;

        for (i = 0; i < 1000; i++) {
            timer_check_timeout();
        }
    }

    return (quit_flag & 0xff);
}

void _app_schedule()
{
    int retval;

    if (pfd_defrag)
        pfd_defragment();
    if ((retval = poll(pfd_list, pfd_count, 0)) > 0) {
        dispatch_sock_events(retval);
    }

    timer_check_timeout();
}

void app_quit(int code)
{
    quit_flag = ((code & 0xFF) | 0x100);
}

struct app_optent {
    const char* name;
    int type;
    int count;
    void* value;
    void* vector;
    struct app_optent* next;
};

static struct app_optent* app_optlist = NULL;

static const struct app_option* find_option(const struct app_option* options, const char* name, int n)
{
    while (options->long_name) {
        if (n > 0) {
            if (strlen(options->long_name) == n && !strncmp(options->long_name, name, n)) {
                return options;
            }
        } else {
            if (name && !strcmp(options->long_name, name)) {
                return options;
            }
        }
        options++;
    }
    if (name == NULL)
        return options;

    return 0;
}

static const struct app_option* find_option_short(const struct app_option* options, char name)
{
    while (options->long_name) {
        if (options->short_name == name) {
            return options;
        }
        options++;
    }

    return 0;
}

static struct app_optent* find_optent(const char* name)
{
    struct app_optent* ent = app_optlist;

    while (ent) {
        if (ent->name == name || (name && ent->name && !strcmp(ent->name, name))) {
            return ent;
        }
        ent = ent->next;
    }

    return 0;
}

#ifdef __APP_GETOPT_DEBUG__
static void dump_optents()
{
    struct app_optent* ent = app_optlist;
    char** slist;
    int* ilist;
    int i;

    while (ent) {
        printf("%s:\n", ent->name);
        if (ent->type & ARG_MULTI) {
            switch ((ent->type & ~ARG_MULTI)) {
            case ARG_NONE:
                printf("  <%d>\n", *((unsigned long*)ent->vector));
                break;
            case ARG_INT:
                i = 0;
                slist = ent->value;
                ilist = ent->vector;
                while (slist[i]) {
                    printf("  %d: \"%s\"<%d>\n", i, slist[i], ilist[i]);
                    i++;
                }
                break;
            default:
                i = 0;
                slist = ent->value;
                while (slist[i]) {
                    printf("  %d: \"%s\"\n", i, slist[i]);
                    i++;
                }
                break;
            }
        } else {
            printf("  \"%s\"\n", ent->value);
        }
        ent = ent->next;
    }
}
#endif

static int parse_list(const char* p, const char** ep, char sep)
{
    int count = 0;

    while (p[count] && p[count] != sep)
        count++;

    if (ep) {
        if (p[count] == sep)
            *ep = &p[count + 1];
        else
            *ep = &p[count];
    }

    return count;
}

static int add_value(struct app_optent* optent, const char* value, int len)
{
    char **list = optent->value, *ptr;
    int i = 0;

    if (!optent->value) {
        if (!(list = malloc(sizeof(char*) * 4))) {
            return -1;
        }
        optent->value = list;
    } else {
        while (list[i])
            i++;
        if ((i & 3) == 0) {
            if (!(list = realloc(list, sizeof(char*) * (i + 4)))) {
                return -1;
            }
            optent->value = list;
        }
    }

    if (len <= 0)
        len = strlen(value);
    if (!(ptr = malloc(len + 1))) {
        return -1;
    }
    memcpy(ptr, value, len);
    ptr[len] = 0;

    list[i++] = ptr;
    list[i] = 0;

    return 0;
}

static int add_long_vector(struct app_optent* optent, const char* value, int len)
{
    long* list = optent->vector;
    char buf[20], *ep;
    int count;

    if (!optent->vector) {
        if (!(list = malloc(sizeof(long int) * 4))) {
            return -1;
        }
        count = 0;
        optent->vector = list;
    } else {
        count = list[0];
        if ((count & 3) == 0) {
            if (!(list = realloc(list, sizeof(long int) * (count + 4)))) {
                return -1;
            }
            optent->vector = list;
        }
    }

    while (isspace(*value))
        value++;
    if (len <= 0)
        len = strlen(value);
    while (isspace(value[len - 1]))
        len--;
    if (!len || len >= 20) {
        return -1;
    }

    memcpy(buf, value, len);
    buf[len] = 0;

    list[count + 1] = strtol(buf, &ep, 0);
    if (ep == buf || *ep != 0) {
        return -1;
    }
    list[0] = ++count;

    return 0;
}

static int set_value(struct app_optent* ent, const char* value)
{
    if (ent->value) {
        free(ent->value);
    }
    ent->value = strdup(value);
    return 0;
}

static int set_long_vector(struct app_optent* ent, long value)
{
    long* lp;

    if (!ent->vector) {
        ent->vector = malloc(sizeof(long));
    }
    lp = ent->vector;
    *lp = value;

    return 0;
}

static int inc_vector(struct app_optent* ent)
{
    long* lp;

    if (!ent->vector) {
        ent->vector = malloc(sizeof(long));
    }
    lp = ent->vector;
    (*lp)++;

    return 0;
}

static int setopt_arg(const struct app_option* option, const char* value)
{
    struct app_optent* ent;

#ifdef __APP_GETOPT_DEBUG
    printf("before set arg %s: %s\n", option->long_name, value);
    dump_optents();
#endif

    if (!(ent = find_optent(option->long_name))) {
        ent = malloc(sizeof(struct app_optent));
        memset(ent, 0, sizeof(struct app_optent));

        if (option->long_name) {
            //ent->name=strdup(option->long_name);
            ent->name = option->long_name;
        }
        ent->type = option->arg_type;
        ent->next = app_optlist;
        app_optlist = ent;
    }

    if (option->arg_type & ARG_MULTI) {
        switch ((option->arg_type & (~ARG_MULTI))) {
        case ARG_NONE: {
            if (inc_vector(ent) < 0) {
                return -1;
            }
        } break;
        case ARG_INT: {
            const char* np;
            int len;

            while (*value) {
                len = parse_list(value, &np, ',');
                if (add_value(ent, value, len) < 0) {
                    return -1;
                }
                if (add_long_vector(ent, value, len) < 0) {
                    return -1;
                }
                value = np;
            }
        } break;
        default: {
            const char* np;
            int len;

            while (*value) {
                len = parse_list(value, &np, ',');
                add_value(ent, value, len);
                value = np;
            }
            ent->vector = ent->value;
        } break;
        }
    } else {
        switch (option->arg_type) {
        case ARG_NONE: {
            set_value(ent, "");
        } break;
        case ARG_INT: {
            char* ep;
            long l;

            l = strtol(value, &ep, 0);
            if (ep == value || *ep != 0) {
                return -1;
            }

            if (set_value(ent, value) < 0) {
                return -1;
            }
            if (set_long_vector(ent, l) < 0) {
                return -1;
            }
        } break;
        default: {
            if (set_value(ent, value) < 0) {
                return -1;
            }
        }
        }
    }

#ifdef __APP_GETOPT_DEBUG
    printf("\nafter set arg %s: %s\n", option->long_name, value);
    dump_optents();
#endif

    return 0;
}

static int parse_args(int argc, char* argv[], const struct app_option* options)
{
    int i = 0;

    while (++i < argc) {
        char* p = argv[i];
        const struct app_option* opt;

        if (*p == '-') {
            p++;
            if (*p == '-') {
                int slen;
                char* ep;

                p++;
                if (*p == '\0') {
                    if (++i < argc) {
                        opt = find_option(options, NULL, 0);
                        if (opt->arg_type == ARG_NONE) {
                            fprintf(stderr, "Warning: no argument needed!\n");
                        }
                        setopt_arg(opt, argv[i]);
                    }
                    continue;
                }

                if ((ep = strchr(p, '=')))
                    slen = ep - p;
                else
                    slen = strlen(p);

                if (!(opt = find_option(options, p, slen))) {
                    return i;
                }

                if ((opt->arg_type & (~ARG_MULTI)) != ARG_NONE) {
                    if (!ep) {
                        if (++i == argc || (*argv[i] == '-' && *(argv[i] + 1) != '\0')) {
                            fprintf(stderr, "option \"%s\" require an argument\n", argv[i]);
                            return -1;
                        } else {
                            if (setopt_arg(opt, argv[i]) < 0) {
                                return -1;
                            }
                        }
                    } else {
                        if (setopt_arg(opt, ep + 1) < 0) {
                            return -1;
                        }
                    }
                } else {
                    if (ep) {
                        fprintf(stderr, "Warning: option \"--%s\" should not have an argument\n", opt->long_name);
                        //return -1;
                    }
                    if (setopt_arg(opt, NULL) < 0) {
                        return -1;
                    }
                }
            } else if (*p == '\0') {
                opt = find_option(options, NULL, 0);
                if (opt->arg_type == ARG_NONE) {
                    fprintf(stderr, "Warning: no argument needed!\n");
                }
                setopt_arg(opt, argv[i]);
            } else {
                while (*p) {
                    if (!(opt = find_option_short(options, *p++))) {
                        return i;
                    }

                    if ((opt->arg_type & (~ARG_MULTI)) != ARG_NONE) {
                        if (*p || i + 1 >= argc) {
                            fprintf(stderr, "option \"-%c\" require an argument\n", *(p - 1));
                            return -1;
                        }
                        setopt_arg(opt, argv[++i]);
                    } else {
                        if (setopt_arg(opt, "") < 0) {
                            return -1;
                        }
                    }
                }
            }
        } else {
            opt = find_option(options, NULL, 0);
            if (opt->arg_type == ARG_NONE) {
                fprintf(stderr, "Warning: no argument needed!\n");
            }
            setopt_arg(opt, argv[i]);
        }
    }

    return 0;
}

static void parse_config_file(const struct app_option* options, FILE* fp)
{
    char buf[1024];
    int lineno = 0;

    memset(buf, 0, sizeof(buf));
    while (fgets(buf, sizeof(buf) - 1, fp)) {
        const struct app_option* option;
        char *p = buf, *pp;

        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '\0')
            continue;
        pp = p + (strlen(p) - 1);
        while (isspace(*pp))
            pp--;
        *pp = '\0';

        pp = strchr(p, '=');
        if (pp) {
            char* tp = pp - 1;
            *pp++ = '\0';
            while (isspace(*tp))
                *tp-- = 0;
        }

        option = find_option(options, p, 0);
        if (!option) {
            fprintf(stderr, "invalid line %d\n", lineno);
            continue;
        }

        if (!find_optent(option->long_name))
            setopt_arg(option, pp);
    }
}

static void load_config_file(const struct app_option* options, const char* appname)
{
    const struct app_option *cfgfile = NULL, *p = options;
    FILE* fp;

    while (p->long_name) {
        if (p->arg_type == ARG_CFGFILE) {
            cfgfile = p;
            break;
        }
        p++;
    }

    if (cfgfile && app_getopt(cfgfile->long_name, APP_GETOPT_INDEX, 0)) {
        const char* fn;
        int i = 0;

        while ((fn = app_getopt(cfgfile->long_name, APP_GETOPT_INDEX, i++))) {
            fp = fopen(fn, "r");
            if (fp) {
                parse_config_file(options, fp);
                fclose(fp);
            }
        }
    } else {
        char fn[128];
        sprintf(fn, "%s/.%src", getenv("HOME"), appname);
        fp = fopen(fn, "r");
        if (fp) {
            parse_config_file(options, fp);
            fclose(fp);
        }

        sprintf(fn, "/etc/%s.conf", appname);
        fp = fopen(fn, "r");
        if (fp) {
            parse_config_file(options, fp);
            fclose(fp);
        }
    }
}

static void load_syscfg(const struct app_option* options)
{
    const struct app_option* option = options;

    while (option->long_name) {
        if (option->equiv_env) {
            char* value;
            value = getsyscfg(option->equiv_env);
            if (value && *value != '\0') {
                struct app_optent* ent;

                ent = find_optent(option->long_name);
                if (!ent || !ent->value) {
                    setopt_arg(option, value);
                }
            }
        }
        option++;
    }
}

void* app_getoptv(const char* optname)
{
    struct app_optent* ent;

    if (!(ent = find_optent(optname))) {
        return NULL;
    }

    return ent->vector;
}

const char* app_getopt(const char* optname, int flags, ...)
{
    struct app_optent* ent;

    ent = find_optent(optname);
    if (flags & APP_GETOPT_COUNT) {
        va_list ap;
        int* count_p;

        va_start(ap, flags);
        count_p = va_arg(ap, int*);
        va_end(ap);

        *count_p = ent ? ent->count : 0;
    }

    if (!ent || !ent->value) {
        return NULL;
    }

    if (flags & APP_GETOPT_INDEX) {
        va_list ap;
        int index;

        va_start(ap, flags);
        index = va_arg(ap, int);
        va_end(ap);

        if (ent->type & ARG_MULTI) {
            char** list = ent->value;
            int i;

            for (i = 0; i < index; i++)
                if (list[i] == NULL)
                    return NULL;
            return list[i];
        } else {
            if (index == 0) {
                return ent->value;
            } else {
                return NULL;
            }
        }
    } else {
        if (ent->type & ARG_MULTI) {
            char** list = ent->value;
            if (list) {
                return list[0];
            } else {
                return NULL;
            }
        } else {
            return ent->value;
        }
    }

    return ent->value;
}

int app_opt_isset(const char* optname)
{
    const char* value;
    char* ep;
    long i;

    if (!(value = app_getopt(optname, 0))) {
        DLOG("%s not set", optname);
        return 0;
    }

    if (!strcasecmp(value, "true")) {
        DLOG("%s is set", optname);
        return 1;
    }

    i = strtol(value, &ep, 0);
    if (ep == value || i == 0) {
        DLOG("%s not set", optname);
        return 0;
    }

    DLOG("%s is set", optname);
    return 1;
}
