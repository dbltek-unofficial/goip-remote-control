#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __USE_GNU
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include "btrace.h"

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
    struct _fpstate* fpstate;
    unsigned long oldmask;
    unsigned long cr2;
};
#define SIG_HANDLER_ARGLIST int sig, struct _sigctxt sc
#endif

#else
#define SIG_HANDLER_ARGLIST int sig
#endif

static void sighandler(SIG_HANDLER_ARGLIST)
{
    const char* signame;
    char buf[32];

    signal(sig, SIG_DFL);

    write(2, "sighandler: ", 12);

    switch (sig) {
    case SIGILL:
        signame = "Illegal instruction\n";
        break;
    case SIGSEGV:
        signame = "Sigment fault\n";
        break;
    case SIGABRT:
        signame = "Aborted\n";
        break;
    case SIGTERM:
        signame = "Terminated\n";
        break;
    case SIGBUS:
        signame = "Bus error\n";
        break;
    default:
        memcpy(buf, "Kill by signal ", 15);
        if (sig > 10) {
            buf[15] = '0' + sig / 10;
            buf[16] = '0' + sig % 10;
            buf[17] = '\n';
            buf[18] = 0;
        } else {
            buf[15] = '0' + sig % 10;
            buf[16] = '\n';
            buf[17] = 0;
        }
        signame = buf;
        break;
    }
    write(2, signame, strlen(signame));

#if 1
#ifdef __linux__
    {
        void* stack_buf[20];
        int n = 0, i;
#ifdef __arm__
        stack_buf[0] = (void*)sc.arm_pc;
        n++;
        n += btrace_from((void*)sc.arm_fp, (void*)sc.arm_sp, stack_buf + 1, sizeof(stack_buf) / sizeof(void*) - 1);
#endif
#ifdef __i386__
        stack_buf[0] = (void*)sc.eip;
        n++;
        n += btrace_from((void*)sc.ebp, (void*)sc.esp, stack_buf + 1, sizeof(stack_buf) / sizeof(void*) - 1);
#endif
        for (i = 0; i <= n; i++) {
            char* p = buf;
            unsigned long ptr = (unsigned long)stack_buf[i];
            int d, shift = 28;

            *p++ = '#';
            if (i > 10)
                *p++ = '0' + i / 10;
            *p++ = '0' + i % 10;
            *p++ = ':';
            *p++ = ' ';

            *p++ = '0';
            *p++ = 'x';
            while (shift >= 0) {
                d = (ptr >> shift) & 0xf;
                if (d >= 10)
                    *p++ = 'a' + d - 10;
                else
                    *p++ = '0' + d;
                shift -= 4;
            }
            *p++ = '\n';
            *p++ = '\0';

            write(2, buf, strlen(buf));
        }
    }
#endif
#endif
    kill(getpid(), sig);
    //_exit(0x80 | sig);
}

void signal_handler_init(void)
{
    //DLOG("signal init!");
    signal(SIGILL, (sighandler_t)sighandler);
    signal(SIGSEGV, (sighandler_t)sighandler);
    signal(SIGABRT, (sighandler_t)sighandler);
    //signal(SIGTERM, (sighandler_t)sighandler);
    signal(SIGBUS, (sighandler_t)sighandler);
}
