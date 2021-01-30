#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <errno.h>
#include <signal.h>

#include "app.h"
#include "btrace.h"
#include "debug.h"
#include "md5.h"
#include "net.h"

//static unsigned long _debug_level_flags=0xFFFFFFFF;
//static FILE *_debug_log=0;
//
#ifdef BUFFER_DEBUG_MSG
#define LOGBUFSIZ 4096
#define LOGBUFMASK (LOGBUFSIZ - 1)

static char logbuf[LOGBUFSIZ];
static unsigned logbuf_head = 0, logbuf_tail = 0;
#endif
static int loglevel = 0;
static int logfd = 2;

extern char** environ;

#define tcp_gets(sock, buf, bufsiz) _tcp_gets(sock, buf, bufsiz, __LINE__)
static int _tcp_gets(int sock, char* buf, int bufsiz, int line)
{
    int i = 0;

    fprintf(stderr, "tcp_gets(): %d\n", line);
    while (i < bufsiz) {
        if (read(sock, &buf[i], 1) != 1) {
            return -1;
        }
        if (buf[i] == '\n') {
            buf[i] = 0;
            fprintf(stderr, "tcp_gets(): %s\n", buf);
            break;
        }
        i++;
    }

    return i;
}

#define HELO "HELO fspipsev.net\r\n"
#define MAIL_FROM "MAIL FROM:<bug@fspipsev.net>\r\n"
#define RCPT_TO "RCPT TO:<bug@fspipsev.net>\r\n"
#define DATA "DATA\r\n"
#define QUIT "QUIT\r\n"

#define BUGRP_SVRERR fprintf(stderr, "bugreport: E%.03d: server error: %s\n", __LINE__, bugrpbuf)
#define BUGRP_CONERR fprintf(stderr, "bugreport: E%.03d: connection error\n", __LINE__)

#if 0
static char bugrpbuf[LOGBUFSIZ+4096];
int bugreport()
{
	struct sockaddr_in addr;
	int sock, res, i=0;

	if(gethostaddr("fspipsev.net", (struct sockaddr*)&addr, sizeof(addr))<0){
		addr.sin_family=AF_INET;
		inet_aton("61.141.247.7", &addr.sin_addr);
	}
	addr.sin_port=htons(25); /* smtp port */

	if((sock=tcp_connect(-1, (struct sockaddr*)&addr, sizeof(addr)))<0){
		fprintf(stderr, "bugreport: E%.03d cannot connect to server!\n", __LINE__);
		return -1;
	}	

	if((res=tcp_gets(sock, bugrpbuf, sizeof(bugrpbuf)-1))<0){
		BUGRP_CONERR;
		return -1;
	}
	bugrpbuf[res]=0;

	if(strncmp(bugrpbuf, "220", 3)){
		BUGRP_SVRERR;
		return -1;
	}

	/* HELO */
	if(write(sock, HELO, sizeof(HELO)-1)<0){
		BUGRP_CONERR;
		return -1;
	}
	
	if((res=tcp_gets(sock, bugrpbuf, sizeof(bugrpbuf)-1))<0){
		BUGRP_CONERR;
		return -1;
	}
	bugrpbuf[res]=0;

	if(strncmp(bugrpbuf, "250", 3)){
		BUGRP_SVRERR;
		return -1;
	}

	/* MAIL FROM */
	if(write(sock, MAIL_FROM, sizeof(MAIL_FROM)-1)<0){
		BUGRP_CONERR;
		return -1;
	}

	if((res=tcp_gets(sock, bugrpbuf, sizeof(bugrpbuf)-1))<0){
		BUGRP_CONERR;
		return -1;
	}
	bugrpbuf[res]=0;

	if(strncmp(bugrpbuf, "250", 3)){
		BUGRP_SVRERR;
		return -1;
	}

	/* RCPT TO */
	if(write(sock, RCPT_TO, sizeof(RCPT_TO)-1)<0){
		BUGRP_CONERR;
		return -1;
	}

	if((res=tcp_gets(sock, bugrpbuf, sizeof(bugrpbuf)-1))<0){
		BUGRP_CONERR;
		return -1;
	}
	bugrpbuf[res]=0;

	if(strncmp(bugrpbuf, "250", 3)){
		BUGRP_SVRERR;
		return -1;
	}

	/* DATA */
	if(write(sock, DATA, sizeof(DATA)-1)<0){
		BUGRP_CONERR;
		return -1;
	}

	if((res=tcp_gets(sock, bugrpbuf, sizeof(bugrpbuf)-1))<0){
		BUGRP_CONERR;
		return -1;
	}
	bugrpbuf[res]=0;

	if(strncmp(bugrpbuf, "354", 3)){
		BUGRP_SVRERR;
		return -1;
	}

	/* the report */
        if(logbuf_tail>LOGBUFSIZ){
		logbuf_tail-=LOGBUFSIZ;
		res=((~logbuf_tail)&LOGBUFMASK)+1;
		memcpy(bugrpbuf, logbuf+(logbuf_tail&LOGBUFMASK), res);
		memcpy(bugrpbuf+res, logbuf, LOGBUFSIZ-res);
		res=LOGBUFSIZ;
	}
	else{
		memcpy(bugrpbuf, logbuf, logbuf_tail);
		res=logbuf_tail;
	}	

	bugrpbuf[res++]='\n';
	bugrpbuf[res++]='\n';

	while(environ[i]){
		memcpy(bugrpbuf+res, environ[i], strlen(environ[i]));
		res+=strlen(environ[i]);
		bugrpbuf[res++]='\n';
		i++;
	}
	bugrpbuf[res++]='\n';
	bugrpbuf[res++]='\n';

	memcpy(bugrpbuf+res, ".\r\n", 3);
	res+=3;

	fwrite(bugrpbuf, res, 1, stderr);
	if(write(sock, bugrpbuf, res)<0){
		BUGRP_CONERR;
		return -1;
	}

	if((res=tcp_gets(sock, bugrpbuf, sizeof(bugrpbuf)-1))<0){
		BUGRP_CONERR;
		return -1;
	}
	bugrpbuf[res]=0;

	if(strncmp(bugrpbuf, "250", 3)){
		BUGRP_SVRERR;
		return -1;
	}

	write(sock, QUIT, sizeof(QUIT));
	tcp_gets(sock, bugrpbuf, sizeof(bugrpbuf)-1);

	close(sock);

	return 0;
}
#endif

void flush_log_msg()
{
#ifdef BUFFER_DEBUG_MSG
    int len;

    len = logbuf_tail - logbuf_head;
    if (len > LOGBUFSIZ) {
        fprintf(stderr, "log buffer overflow!\n");
        logbuf_head = logbuf_tail - LOGBUFSIZ;
        len = LOGBUFSIZ;
    }

    while (len > 0) {
        int count = ((~logbuf_head) & LOGBUFMASK) + 1;

        if (len < count)
            count = len;
        if ((count = write(logfd, &logbuf[logbuf_head & LOGBUFMASK], count)) < 0) {
            perror("flush_log_msg()");
            return;
        }
        len -= count;
        logbuf_head += count;
    }
#endif
    fsync(logfd);
}

#ifdef BUFFER_DEBUG_MSG
static void write_log_msg()
{
    int len, count;

    len = logbuf_tail - logbuf_head;
    if (len > LOGBUFSIZ) {
        fprintf(stderr, "log buffer overflow!\n");
        logbuf_head = logbuf_tail - LOGBUFSIZ;
        len = LOGBUFSIZ;
    }

    count = ((~logbuf_head) & LOGBUFMASK) + 1;
    if (len < count)
        count = len;
    if ((count = write(logfd, &logbuf[logbuf_head & LOGBUFMASK], count)) < 0) {
        return;
    }
    //fprintf(stderr, "write_log_msg(): %d\n", count);
    logbuf_head += count;

    if (logbuf_head == logbuf_tail) {
        unregister_sock(logfd, 0, write_log_msg);
    }
}
#endif

static void log_msg(char* logmsg, int len)
{
#ifdef BUFFER_DEBUG_MSG
    struct stat st;

    if (fstat(logfd, &st) < 0) {
        st.st_mode = 0;
    }

    while (len > 0) {
        int count = ((~logbuf_tail) & LOGBUFMASK) + 1;

        if (len < count)
            count = len;
        memcpy(&logbuf[logbuf_tail & LOGBUFMASK], logmsg, count);
        if (S_ISREG(st.st_mode)) {
            write(logfd, logmsg, count);
        }
        len -= count;
        logmsg += count;
        logbuf_tail += count;
    }

    if (S_ISCHR(st.st_mode) || S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode)) {
        //fprintf(stderr, "log output is a %s\n", S_ISCHR(st.st_mode)?"device":(S_ISFIFO(st.st_mode)?"fifo":"socket"));
        register_sock(logfd, SOCKOUT, 0, write_log_msg);
    }
#else
    write(logfd, logmsg, len);
#endif
}

void dprint(int level, const char* fmt, ...)
{
    if (level >= loglevel) {
        va_list ap;
        int res, errno_save;
        char buf[256];

        errno_save = errno;

        va_start(ap, fmt);
        res = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
        va_end(ap);

        if (res > 0) {
            log_msg(buf, res);
        }

        errno = errno_save;
    }
}

/*
int _dprint(int level, const char *fmt, ...)
{
	unsigned long m=1;
	va_list ap;
	int res, errno_save;
	
	if(level>31)level=31;
	m<<=level;
	if(!(m&_debug_level_flags)){
		return 0;
	}

	errno_save = errno;

	va_start(ap, fmt);
	if(!_debug_log)_debug_log=stderr;
	res=vfprintf(_debug_log, fmt, ap);
	va_end(ap);

	fflush(_debug_log);

	errno = errno_save;
	return res;
}
*/

void dlog(int level, const char* func, const char* file, int line, const char* fmt, ...)
{
    if (level >= loglevel) {
        char buf[256];
        char buf2[300];
        va_list ap;
        int n, save_errno;

        save_errno = errno;

        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
        va_end(ap);
        buf[sizeof(buf) - 1] = 0;

        if ((n = snprintf(buf2, sizeof(buf2) - 1, "  %s(): %s: %d: %s\n", func, file, line, buf)) > 0) {
            log_msg(buf2, n);
        }

        errno = save_errno;
    }
}

void logsyserr(int level, const char* func, const char* file, int line)
{
    if (level >= loglevel) {
        char buf[256];
        int n, save_errno;

        save_errno = errno;

        n = snprintf(buf, sizeof(buf) - 1, "  SYSERROR: %s(): %s: %d: %s\n", func, file, line, strerror(errno));
        log_msg(buf, n);

        /*
		if(isatty(logfd)){
			n=snprintf(buf, sizeof(buf)-1, "  \033[1;33mSYSERROR\033[0;39m: %s(): %s: %d: %s\n", func, file, line, strerror(errno));
		}
		write(logfd, buf, n);
		fdatasync(logfd);
		*/

        errno = save_errno;
    }
}

void logfail(int level, const char* func, const char* file, int line)
{
    if (level >= loglevel) {
        char buf[256];
        int n, save_errno;

        save_errno = errno;

        n = snprintf(buf, sizeof(buf) - 1, "  FAILED: %s(): %s: %d\n", func, file, line);
        log_msg(buf, n);

        /*
		if(isatty(logfd)){
			n=snprintf(buf, sizeof(buf)-1, "  \033[1;31mFAILED\033[0;39m: %s(): %s: %d\n", func, file, line);
		}
		write(logfd, buf, n);
		fdatasync(logfd);
		*/

        errno = save_errno;
    }
    //dprint(level, "  FAILED: %s(): %s: %d\n", func, file, line);
}

void checkpoint(int level, const char* func, const char* file, int line)
{
    if (level >= loglevel) {
        char buf[256];
        int n, save_errno;

        save_errno = errno;

        n = snprintf(buf, sizeof(buf) - 1, "  CHECKPOINT: %s(): %s: %d\n", func, file, line);
        log_msg(buf, n);

        /*
		if(isatty(logfd)){
			n=snprintf(buf, sizeof(buf)-1, "  \033[1;32mCHECKPOINT\033[0;39m: %s(): %s: %d\n", func, file, line);
		}
		write(logfd, buf, n);
		fdatasync(logfd);
		*/

        errno = save_errno;
    }
    //dprint(level, "  CHECK POINT: %s(): %s: %d\n", func, file, line);
}

int set_debug_level(int level)
{
    if (level > 0)
        loglevel = level;
    return loglevel;
}

int set_log_file(const char* file_name)
{
    int fd;

    if ((fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) >= 0) {
        logfd = fd;
        return 0;
    }
    return -1;
}
int set_log_file_app(const char* file_name)
{
    int fd;

    if ((fd = open(file_name, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) >= 0) {
        logfd = fd;
        return 0;
    }
    return -1;
}

#ifdef MEMDEBUG

#ifdef malloc
#undef malloc
#endif

#ifdef free
#undef free
#endif

#define PAGE_SIZE 4096
#define DMZ_SIZE 64

#define ALIGN 4L
#define ALIGN_SIZE(size) ((size + ALIGN - 1) & (~(ALIGN - 1)))

struct mem_debug_blk_hdr {
    unsigned char magic[4];
    char info[256];
    void* stack[20];
    size_t size;
    struct mem_debug_blk_hdr* next;
    struct mem_debug_blk_hdr* prev;
    int free;
#ifndef MEMLEAK_DEBUG_ONLY
    unsigned char check_sum[16];
    int dmz[DMZ_SIZE];
#endif
};

#ifndef MEMLEAK_DEBUG_ONLY
struct mem_debug_blk_end {
    int dmz[DMZ_SIZE];
    int free;
    struct mem_debug_blk_end* prev;
    struct mem_debug_blk_end* next;
    size_t size;
    void* stack[20];
    char info[256];
    unsigned char check_sum[16];
    unsigned char magic[4];
};
#endif

static struct mem_debug_blk_hdr* first_blk_hdr = 0;
static struct mem_debug_blk_hdr* last_blk_hdr = 0;
#ifndef MEMLEAK_DEBUG_ONLY
static struct mem_debug_blk_end* first_blk_end = 0;
static struct mem_debug_blk_end* last_blk_end = 0;
#endif

unsigned char padseq[3] = { 0xAF, 0x71, 0x80 };

#ifndef MEMLEAK_DEBUG_ONLY
static void dmz_init(int dmz[])
{
    struct timeval tv;
    int i;

    gettimeofday(&tv, 0);
    srandom(tv.tv_sec ^ tv.tv_usec);

    for (i = 0; i < DMZ_SIZE; i++) {
        dmz[i] = random();
    }
}

static void mem_debug_calc_hdr_check_sum(struct mem_debug_blk_hdr* hdr, unsigned char check_sum[16])
{
    MD5_CTX ctx;
    MD5Init(&ctx);
    MD5Update(&ctx, hdr->magic, sizeof(hdr->magic));
    MD5Update(&ctx, &hdr->size, sizeof(hdr->size));
    MD5Update(&ctx, &hdr->info, sizeof(hdr->info));
    MD5Update(&ctx, &hdr->prev, sizeof(hdr->prev));
    MD5Update(&ctx, &hdr->next, sizeof(hdr->next));
    MD5Update(&ctx, &hdr->dmz, sizeof(hdr->dmz));
    MD5Final(check_sum, &ctx);
}
#endif

static void mem_debug_init_hdr(struct mem_debug_blk_hdr* hdr, int size, char* info)
{

    memcpy(hdr->magic, "mdbh", 4);
    hdr->size = size;
    hdr->free = 0;
    bzero(hdr->info, sizeof(hdr->info));
    strncpy(hdr->info, info, sizeof(hdr->info));

#ifndef MEMLEAK_DEBUG_ONLY
    dmz_init(hdr->dmz);
#endif

    hdr->next = 0;
    hdr->prev = last_blk_hdr;
    if (!last_blk_hdr) {
        first_blk_hdr = hdr;
    } else {
        last_blk_hdr->next = hdr;
#ifndef MEMLEAK_DEBUG_ONLY
        mem_debug_calc_hdr_check_sum(last_blk_hdr, last_blk_hdr->check_sum);
#endif
    }

    last_blk_hdr = hdr;

#ifndef MEMLEAK_DEBUG_ONLY
    mem_debug_calc_hdr_check_sum(hdr, hdr->check_sum);
#endif
}

static int mem_debug_check_hdr(struct mem_debug_blk_hdr* hdr)
{
    if (memcmp(hdr->magic, "mdbh", 4)) {
        //		LOG_FUNCTION_FAILED;
        return 0;
    }

#ifndef MEMLEAK_DEBUG_ONLY
    {
        unsigned char check_sum[16];

        mem_debug_calc_hdr_check_sum(hdr, check_sum);

        if (memcmp(check_sum, hdr->check_sum, sizeof(check_sum))) {
            return 0;
        }
    }
#endif

    //	LOG_FUNCTION_FAILED;
    return 1;
}

#ifndef MEMLEAK_DEBUG_ONLY
static void mem_debug_calc_end_check_sum(struct mem_debug_blk_end* end, unsigned char check_sum[16])
{
    MD5_CTX ctx;

    MD5Init(&ctx);
    MD5Update(&ctx, end->magic, sizeof(end->magic));
    MD5Update(&ctx, &end->size, sizeof(end->size));
    MD5Update(&ctx, &end->info, sizeof(end->info));
    MD5Update(&ctx, &end->prev, sizeof(end->prev));
    MD5Update(&ctx, &end->next, sizeof(end->next));
    MD5Update(&ctx, &end->dmz, sizeof(end->dmz));
    MD5Final(check_sum, &ctx);
}

static void mem_debug_init_end(struct mem_debug_blk_end* end, int size, char* info)
{
    memcpy(end->magic, "mdbe", 4);
    end->size = size;
    end->free = 0;
    bzero(end->info, sizeof(end->info));
    strncpy(end->info, info, sizeof(end->info));

    dmz_init(end->dmz);

    end->next = 0;
    end->prev = last_blk_end;
    if (!last_blk_end) {
        first_blk_end = end;
    } else {
        last_blk_end->next = end;
        mem_debug_calc_end_check_sum(last_blk_end, last_blk_end->check_sum);
    }
    last_blk_end = end;

    mem_debug_calc_end_check_sum(end, end->check_sum);
}

static int mem_debug_check_end(struct mem_debug_blk_end* end)
{
    unsigned char check_sum[16];

    if (memcmp(end->magic, "mdbe", 4)) {
        //		LOG_FUNCTION_FAILED;
        return 0;
    }

    mem_debug_calc_end_check_sum(end, check_sum);

    if (!memcmp(check_sum, end->check_sum, sizeof(check_sum))) {
        int pad = ALIGN_SIZE(end->size) - end->size;

        if (pad) {
            int i;
            unsigned char* ptr = ((unsigned char*)end) - pad;

            for (i = 0; i < pad; i++) {
                if (ptr[i] != padseq[i]) {
                    //					DPRINT(__FUNCTION__"(): %X:%X\n",ptr[i], padseq[i]);
                    //					LOG_FUNCTION_FAILED;
                    return 0;
                }
            }
        }
        return 1;
    }

    return 0;
}
#endif

#define __MEMDEBUG__ "memory debugger: "

void __memdebug_print();

void __memdebug_check()
{
#ifndef MEMLEAK_DEBUG_ONLY
    struct mem_debug_blk_hdr* hdr;
    struct mem_debug_blk_end* end;
    int i;

    //	__memdebug_print();

    for (hdr = first_blk_hdr, end = first_blk_end; hdr && end; hdr = hdr->next, end = end->next) {
        if (!mem_debug_check_hdr(hdr)) {
            if (!mem_debug_check_end(end)) {
                fprintf(stderr, __MEMDEBUG__ "fatal: memory block corrupted in both sides! try to increate the dmz size!\n");
                fprintf(stderr, __MEMDEBUG__ "%s: size=%d, ptr=%p\n", end->info, end->size, ((unsigned char*)end) - ALIGN_SIZE(end->size));
                fprintf(stderr, __MEMDEBUG__ "%s: size=%d, ptr=%p\n", hdr->info, hdr->size, (unsigned char*)(hdr + 1));
            } else {
                fprintf(stderr, __MEMDEBUG__ "%s: size=%d, ptr=%p: memory overflow!\n", end->info, end->size, ((unsigned char*)end) - ALIGN_SIZE(end->size));
            }
            for (i = 0; i < 20 && hdr->stack[i]; i++) {
                fprintf(stderr, "	%s[%p]\n", addr_to_name(hdr->stack[i]), hdr->stack[i]);
            }
            raise(SIGQUIT);
        }

        if (!mem_debug_check_end(end)) {
            fprintf(stderr, __MEMDEBUG__ "%s: size=%d, ptr=%p: memory underflow!\n", hdr->info, hdr->size, (unsigned char*)(hdr + 1));
            for (i = 0; i < 20 && end->stack[i]; i++) {
                fprintf(stderr, "	%s[%p]\n", addr_to_name(end->stack[i]), end->stack[i]);
            }
            raise(SIGQUIT);
        }
    }
#endif
}

void __memdebug_print()
{
    struct mem_debug_blk_hdr* hdr;
    int count = 0, total = 0, i;

    fprintf(stderr, __MEMDEBUG__ "allocated memory list:\n");
    for (hdr = first_blk_hdr; hdr; hdr = hdr->next) {
        //fprintf(stderr, "\t%s: size=%d, ptr=%p: prev=%s next=%s\n", hdr->info, hdr->size, (unsigned char *)(hdr+1), hdr->prev?hdr->prev->info:"0", hdr->next?hdr->next->info:"0");
        fprintf(stderr, "#%d: %s: size=%d, ptr=%p\n", count, hdr->info, hdr->size, (unsigned char*)(hdr + 1));
        count++;
        total += hdr->size;
        for (i = 0; i < 20 && hdr->stack[i]; i++) {
            fprintf(stderr, "	%s[%p]\n", addr_to_name(hdr->stack[i]), hdr->stack[i]);
        }
    }
    fprintf(stderr, "count=%d total=%d(%dK)\n", count, total, total / 1024);
}

static int __memdebug_enable = 0, __malloc_hook_initialized = 0;

#include <malloc.h>

/* prototypes for our hooks.  */
static void my_init_hook(void);
static void* my_malloc_hook(size_t, const void*);
static void my_free_hook(void* ptr, const void* caller);

/* variables to save original hooks. */
static void* (*old_malloc_hook)(size_t, const void*);
static void (*old_free_hook)(void*, const void*);

/* override initialising hook from the c library. */
void (*__malloc_initialize_hook)(void) = my_init_hook;

void* __memdebug_alloc(size_t size, const void* caller)
{
    static int __in_memdebug_alloc = 0;
    struct mem_debug_blk_hdr* hdr;
#ifndef MEMLEAK_DEBUG_ONLY
    struct mem_debug_blk_end* end;
#endif
    unsigned char* ptr;
    char info[256];
    int pad, i;

    //CHECKPOINT;
    if (__in_memdebug_alloc) {
        raise(SIGQUIT);
    }
    __in_memdebug_alloc = 1;

    if (!__memdebug_enable) {
        return malloc(size);
    }

    if (size > MEM_ALLOC_MAX) {
        //char buf[1024];
        void* stack[20];
        int len, i;
        //len=sprintf(buf, __MEMDEBUG__"%s: malloc too much memory: %d!\n", addr_to_name(caller), size);
        //write(2, buf, len);
        len = fprintf(stderr, __MEMDEBUG__ "%s: malloc too much memory: %d!\n", addr_to_name(caller), size);

        len = btrace(stack, 20);
        for (i = 0; i < len; i++) {
            fprintf(stderr, "	%s[%p]\n", addr_to_name(stack[i]), stack[i]);
        }

        raise(SIGQUIT);
    }

#ifndef MEMLEAK_DEBUG_ONLY
    __memdebug_check();
#endif

    pad = ALIGN_SIZE(size) - size;
    //fprintf(stderr, "malloc(%d, %p) pad=%d\n", size, caller, pad);
#ifndef MEMLEAK_DEBUG_ONLY
    if (!(hdr = malloc(sizeof(struct mem_debug_blk_hdr) + sizeof(struct mem_debug_blk_end) + size + pad))) {
#else
    if (!(hdr = malloc(sizeof(struct mem_debug_blk_hdr) + size + pad))) {
#endif
        char buf[1024];
        int len;
        len = sprintf(buf, __MEMDEBUG__ "%s: not enought memory!\n", addr_to_name(caller));
        write(2, buf, len);
        raise(SIGQUIT);
    }

    ptr = (unsigned char*)(hdr + 1);
#ifndef MEMLEAK_DEBUG_ONLY
    end = (struct mem_debug_blk_end*)(ptr + ALIGN_SIZE(size));
#endif
    snprintf(info, sizeof(info), __MEMDEBUG__ "malloc call from: %s", addr_to_name(caller));

    for (i = 0; i < pad; i++) {
        ptr[size + i] = padseq[i];
    }

    //	__memdebug_check();

    bzero(hdr->stack, sizeof(hdr->stack));
    btrace(hdr->stack, 20);
    mem_debug_init_hdr(hdr, size, info);

#ifndef MEMLEAK_DEBUG_ONLY
    bzero(end->stack, sizeof(end->stack));
    btrace(end->stack, 20);
    mem_debug_init_end(end, size, info);
#endif

    //	fprintf(stderr, __MEMDEBUG__"%s: %d: %s(): malloc(%s): size=%d ptr=%p hdr=%p end=%p\n", file, lineno, func, size_code, size, ptr, hdr, end);

    __in_memdebug_alloc = 0;
    return ptr;
}

static struct mem_debug_blk_hdr* mem_debug_find_blk(void* ptr)
{
    struct mem_debug_blk_hdr* hdr = ((struct mem_debug_blk_hdr*)ptr) - 1;

    if (!mem_debug_check_hdr(hdr)) {
        //		LOG_FUNCTION_FAILED;
        return 0;
    }

    return hdr;
}

void mem_debug_free_blk(struct mem_debug_blk_hdr* hdr)
{
#ifndef MEMLEAK_DEBUG_ONLY
    struct mem_debug_blk_end* end;

    __memdebug_check();

    end = (struct mem_debug_blk_end*)(((char*)(hdr + 1)) + ALIGN_SIZE(hdr->size));

    if (hdr->free > 0 || end->free > 0) {
        int i, n;

        fprintf(stderr, "free again!\n");
        fprintf(stderr, "alloc from\n");
        for (i = 0; i < 20 && hdr->stack[i]; i++) {
            fprintf(stderr, "	%s[%p]\n", addr_to_name(hdr->stack[i]), hdr->stack[i]);
        }

        fprintf(stderr, "first free at\n");
        for (i = 0; i < 20 && end->stack[i]; i++) {
            fprintf(stderr, "	%s[%p]\n", addr_to_name(end->stack[i]), end->stack[i]);
        }

        n = btrace(hdr->stack, 20);
        fprintf(stderr, "free again at\n");
        for (i = 0; i < n; i++) {
            fprintf(stderr, "	%s[%p]\n", addr_to_name(hdr->stack[i]), hdr->stack[i]);
        }

        raise(SIGQUIT);
    }

#endif

    if (!hdr->prev) {
        first_blk_hdr = hdr->next;
    } else {
        hdr->prev->next = hdr->next;
#ifndef MEMLEAK_DEBUG_ONLY
        mem_debug_calc_hdr_check_sum(hdr->prev, hdr->prev->check_sum);
#endif
    }
    hdr->free++;

#ifndef MEMLEAK_DEBUG_ONLY
    if (!end->prev) {
        first_blk_end = end->prev;
    } else {
        end->prev->next = end->next;
        mem_debug_calc_end_check_sum(end->prev, end->prev->check_sum);
    }
#endif

    if (!hdr->next) {
        last_blk_hdr = hdr->prev;
    } else {
        hdr->next->prev = hdr->prev;
#ifndef MEMLEAK_DEBUG_ONLY
        mem_debug_calc_hdr_check_sum(hdr->next, hdr->next->check_sum);
#endif
    }

#ifndef MEMLEAK_DEBUG_ONLY
    if (!end->next) {
        last_blk_end = end->prev;
    } else {
        end->next->prev = end->prev;
        mem_debug_calc_end_check_sum(end->next, end->next->check_sum);
    }
    end->free++;
    btrace(end->stack, 20);
#endif

#ifdef MEMLEAK_DEBUG_ONLY
    free(hdr);
#endif
}

void __memdebug_free(void* ptr, const void* caller)
{
    struct mem_debug_blk_hdr* hdr;

#ifndef MEMLEAK_DEBUG_ONLY
    __memdebug_check();
#endif

    //CHECKPOINT;
    if (!ptr) {
        fprintf(stderr, __MEMDEBUG__ ":%s(%p): try to free null pointer\n", addr_to_name(caller), caller);
        raise(SIGQUIT);
    }

    if (!(hdr = mem_debug_find_blk(ptr))) {
        //		fprintf(stderr, __MEMDEBUG__"ptr=%p: not alloc by __memdebug_alloc()\n", ptr);
        free(ptr);
        return;
    }

    mem_debug_free_blk(hdr);
}

#if 0
char *__memdebug_do_strdup(char *s)
{
	char *sp;

	sp=malloc(strlen(s)+1);
	strcpy(sp, s);

	return sp;
}

char *__memdebug_strdup(const char *s, const char *file, int lineno, const char *func, const char *code)
{
	int len;
	char *ss;
	char codebuf[4096];

	len=strlen(s);
	snprintf(codebuf, sizeof(codebuf), "%s:\"%s\"", code, s);
	code=__memdebug_do_strdup(codebuf);
	if(!(ss=__memdebug_alloc(len+1, strdup))){
		return 0;
	}

	strcpy(ss, s);
	return ss;
}
#endif

static void
my_init_hook(void)
{
    old_malloc_hook = __malloc_hook;
    old_free_hook = __free_hook;
    __malloc_hook_initialized = 1;
    if (__memdebug_enable) {
        __malloc_hook = my_malloc_hook;
        __free_hook = my_free_hook;
    }
}

static void*
my_malloc_hook(size_t size, const void* caller)
{
    void* result;

    /* restore all old hooks */
    __malloc_hook = old_malloc_hook;

    /* call recursively */
    result = __memdebug_alloc(size, caller);

    /* save underlying hooks */
    old_malloc_hook = __malloc_hook;

    /* restore our own hooks */
    __malloc_hook = my_malloc_hook;

    return result;
}

static void
my_free_hook(void* ptr, const void* caller)
{
    /* restore all old hooks */
    __free_hook = old_free_hook;

    /* call recursively */
    __memdebug_free(ptr, caller);

    /* save underlying hooks */
    old_free_hook = __free_hook;

    /* restore our own hooks */
    __free_hook = my_free_hook;
}

void __set_memdebug_enable(int enable)
{
    __memdebug_enable = enable;
    if (__malloc_hook_initialized) {
        if (enable) {
            __free_hook = my_free_hook;
            __malloc_hook = my_malloc_hook;
        } else {
            __malloc_hook = old_malloc_hook;
            __free_hook = old_free_hook;
        }
    }
}

#endif

static void* call_stack[256];
static int call_stack_top = 0;

int log_function_enter(void* func)
{
#ifdef MEMDEBUG
    __memdebug_check();
#endif
    call_stack[call_stack_top++] = func;

    return call_stack_top;
}

void log_function_exit(void* func)
{
    //	assert(call_stack_top>0 && func==call_stack[call_stack_top-1]);
    if (!(call_stack_top > 0 && func == call_stack[call_stack_top - 1]))
        return;

    call_stack_top--;

#ifdef MEMDEBUG
    __memdebug_check();
#endif
    /*	
	struct function_call_log_entry *entry;
	
	if(call_stack_top<=0)
		raise(SIGQUIT);
	
	entry=&call_stack[call_stack_top-1];

	if(strcmp(entry->func, func) || strcmp(entry->file, entry->file)){
		raise(SIGQUIT);
	}

	if(entry->ext_info)
		free(entry->ext_info);

	call_stack_top--;
*/
}

void __debug_dump_logged_call_stack()
{
    int i = 0;

    fprintf(stderr, "Logged Call Stack\n");

    while (call_stack_top > 0) {
        void* p = call_stack[--call_stack_top];
        char* name = addr_to_name(p);

        fprintf(stderr, "\t%d: %s[%p]\n", i, name ? name : "???", p);
        i++;
    }
}
