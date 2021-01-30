#ifndef _DATALIST_H
#define _DATALIST_H

#include "typeinfo.h"
#include "utils.h"

struct datalist {
    struct memblk* memblk;
    int n;
    int ref;
    struct datalist_iterator* first;
    struct datalist_iterator* last;
    struct datalist_iterator* free;
};

struct datalist* create_datalist();
void datalist_init(struct datalist* datalist);

struct datalist* _create_datalist(const char* file, int lineno, const char* func);
void _datalist_init(struct datalist* datalist, const char* file, int lineno, const char* func);
void datalist_ref(struct datalist* datalist);
void datalist_free(struct datalist* datalist);
void* datalist_allocmem(struct datalist* datalist, size_t nbytes);

int datalist_size(struct datalist* datalist);
int datalist_insert(struct datalist* datalist, int index, void* data);
void* datalist_insert_type(struct datalist* datalist, int index, const struct typeinfo* ti);
int datalist_append(struct datalist* datalist, void* data);
void* datalist_append_type(struct datalist* datalist, const struct typeinfo* ti);
int datalist_remove_data(struct datalist* datalist, void* data);
int datalist_remove(struct datalist* datalist, int index);
int datalist_getdata(struct datalist* datalist, int index, void** data_ret);
void* datalist_get(struct datalist* datalist, int index);
int datalist_copy(struct datalist* dest, struct datalist* src);

struct datalist_iterator* datalist_first(struct datalist* datalist);
struct datalist_iterator* datalist_last(struct datalist* datalist);
struct datalist_iterator* datalist_next(struct datalist_iterator* it);
void* datalist_itdata(struct datalist_iterator* it);

TYPEDECL(datalist);

#endif
