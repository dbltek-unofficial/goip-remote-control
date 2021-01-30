#ifndef SYSCFG_H
#define SYSCFG_H

#ifdef __MLIBC__
#include <syscfg.h>
#else
char* getsyscfg(const char* var);
int setsyscfg(const char* var, const char* value);
int syscfg_enum(int (*)(void*, const char*, const char*), void*);
int syscfg_apply();
int syscfg_save();
int syscfg_reload();

#endif
#endif
