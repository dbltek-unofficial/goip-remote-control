#ifndef _TYPE_INFO_H
#define _TYPE_INFO_H

#include <stdarg.h>
#include <sys/types.h>

struct typeinfo {
    const char* name;
    size_t size;
    void* init /* int (*init)(void *, va_list ap) */;
    void* free /* void (*free)(void *) */;
    void* copy /* int (*copy)(void *, void *) */;
};

void* type_create(const struct typeinfo* ti, ...);
void type_free(const struct typeinfo* ti, void*);
int type_copy(const struct typeinfo* ti, void* dest, void* src);
void* type_clone(const struct typeinfo* ti, void* src);

#define TYPEINFO(name) __##name##_typeinfo
#define TYPEDECL(name) extern const struct typeinfo __##name##_typeinfo[];
#define TYPEDEF(type, name, init, free, copy) const struct typeinfo __##name##_typeinfo[] = { { #name, sizeof(type), init, free, copy } }

TYPEDECL(int);

#endif
