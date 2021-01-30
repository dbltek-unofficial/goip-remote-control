#ifndef _DEBUG_H

#ifndef NDEBUG
//#define NDEBUG

#ifndef MEMDEBUG
//#define MEMDEBUG
#endif

#ifndef MEMLEAK_DEBUG_ONLY
//#define MEMLEAK_DEBUG_ONLY
#endif
#endif

#ifndef __DEBUG_FLAGS_ONLY
#define _DEBUG_H

void dprint(int level, const char* fmt, ...);
//int _dprint(int level, const char *fmt, ...);
int set_debug_level(int level);
int set_log_file_app(const char* file_name);
int set_log_file(const char* file_name);
extern void dump(int level, const unsigned char* buf, int len);

int log_function_enter(void* func);
void log_function_exit(void* func);
void __debug_dump_call_stack();

void __set_memdebug_enable(int enable);
void __memdebug_check();
void __memdebug_print();
//void *__memdebug_alloc(size_t size, const char *file, int lineno, const char *func, const char *size_code);
//void __memdebug_free(void *ptr, const char *file, int lineno, const char *func, const char *ptr_code);
char* __memdebug_strdup(const char* s, const char* file, int lineno, const char* func, const char* code);

int bugreport();
void flush_log_msg();

#define MEM_ALLOC_MAX 1024 * 128

#ifdef MEMDEBUG

//#define malloc(size) __memdebug_alloc(size, __FILE__, __LINE__, __FUNCTION__, #size)
//#define free(ptr) __memdebug_free(ptr, __FILE__, __LINE__, __FUNCTION__, #ptr)
//#ifdef strdup
//#undef strdup
//#endif
//#define strdup(s) __memdebug_strdup(s, __FILE__, __LINE__, __FUNCTION__, #s)

#endif

#ifndef NDEBUG

void dlog(int level, const char* func, const char* file, int line, const char* fmt, ...);
void logsyserr(int level, const char* func, const char* file, int line);
void logfail(int level, const char* func, const char* file, int line);
void checkpoint(int level, const char* func, const char* file, int line);
#define DLOG(x...) dlog(DEBUG_LEVEL, __FUNCTION__, __FILE__, __LINE__, x);
#define LOG_SYS_ERROR logsyserr(DEBUG_LEVEL, __FUNCTION__, __FILE__, __LINE__)
#define LOG_FUNCTION_FAILED logfail(DEBUG_LEVEL, __FUNCTION__, __FILE__, __LINE__)
#define CHECKPOINT checkpoint(DEBUG_LEVEL, __FUNCTION__, __FILE__, __LINE__)
//#define LOG_SYS_ERROR dprint(DEBUG_LEVEL, "SYSERROR:"__FUNCTION__"(): " __FILE__ ":%d :%m\n", __LINE__)
//#define LOG_FUNCTION_FAILED dprint(DEBUG_LEVEL, __FUNCTION__"(): failed at "__FILE__":%d\n", __LINE__)
//#define CHECKPOINT dprint(DEBUG_LEVEL, "Check Point:"__FUNCTION__"(): "__FILE__":%d\n", __LINE__)
//#define LOG_ENTER dprint(DEBUG_LEVEL, "ENTER: %s()\n", __FUNCTION__)
//#define LOG_LEAVE dprint(DEBUBLEVEL, "LEAVE: %s()\n", __FUNCTION__)
#define DPRINT(x...) dprint(DEBUG_LEVEL, x)
#define DUMP(buf, len) dump(DEBUG_LEVEL, buf, len)
//#define LOG_FUNCTION_ENTER(info) log_function_enter(__FUNCTION__, __FILE__, __LINE__, info)
//#define LOG_FUNCTION_EXIT log_function_exit(__FUNCTION__, __FILE__, __LINE__)
#define CALL(func, params)                      \
    log_function_enter(func) ? func params : 0; \
    log_function_exit(func)

#else

#define DLOG(x...)
#define LOG_SYS_ERROR
#define LOG_FUNCTION_FAILED
#define CHECKPOINT
#define LOG_ENTER
#define LOG_LEAVE
#define DPRINT(x...)
#define DUMP(buf, len)
#define LOG_FUNCTION_ENTER(info)
#define LOG_FUNCTION_EXIT
#define CALL(func, params...) func params

#endif /* NDEBUG */

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 0
#endif

#else
#undef __DEBUG_FLAGS_ONLY
#endif

#endif /* _DEBUG_H */
