#include "typeinfo.h"
#include <stdlib.h>
#include <string.h>

void* type_create(const struct typeinfo* ti, ...)
{
    void* p;

    if (!(p = malloc(ti->size))) {
        return 0;
    }

    if (ti->init) {
        va_list ap;
        int (*init)(void*, va_list ap);

        va_start(ap, ti);
        init = ti->init;

        if (init(p, ap) < 0) {
            free(p);
            p = 0;
        }
    } else {
        bzero(p, ti->size);
    }

    return p;
}

void type_free(const struct typeinfo* ti, void* p)
{
    if (ti->free) {
        void (*free)(void*);

        free = ti->free;
        free(p);
    }
    free(p);
}

int type_copy(const struct typeinfo* ti, void* dest, void* src)
{
    if (ti->copy) {
        int (*copy)(void*, void*);

        copy = ti->copy;
        return copy(dest, src);
    } else {
        memcpy(dest, src, ti->size);
        return ti->size;
    }
}

void* type_clone(const struct typeinfo* ti, void* src)
{
    void* p;

    if (!(p = malloc(ti->size))) {
        return 0;
    }

    if (type_copy(ti, p, src) < 0) {
        free(p);
        p = 0;
    }

    return p;
}

TYPEDEF(int, int, 0, 0, 0);
