#ifndef BTRACE_H
#define BTRACE_H
/*
 * btrace.h -- debugging utility that provides printings of the backtrace of
 * the current function.
 * $Id: btrace.h,v 1.1.1.1 2012/03/02 02:33:53 elwin Exp $
 */

#include <sys/types.h>

extern int btrace(void** array, int size);
extern int btrace_symbols(const char** array, int size);

extern int btrace_init(void);
extern char* addr_to_name(const void* addr);

#endif /* !BTRACE_H */
