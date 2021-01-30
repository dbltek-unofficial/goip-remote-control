#ifndef _DATASET_H
#define _DATASET_H

#include "typeinfo.h"
#include "utils.h"

struct dataset {
    int ref;
    struct memblk* memblk;
    struct dataset_iterator* first;
    struct dataset_iterator* last;
    struct dataset_iterator* free;
};

struct dataset* create_dataset();
void dataset_init(struct dataset* dataset);

void dataset_free(struct dataset* dataset);
void dataset_ref(struct dataset* dataset);
void* dataset_allocmem(struct dataset* dataset, size_t nbytes);

void* dataset_insert_type(struct dataset* dataset, const char* name, const struct typeinfo* ti);
const struct typeinfo* dataset_get_type(struct dataset* dataset, const char* name);
int dataset_insert_data(struct dataset* dataset, const char* name, void* data);
int dataset_insert_value(struct dataset* dataset, const char* name, long value);
int dataset_insert_string(struct dataset* dataset, const char* name, const char* str);
int dataset_remove(struct dataset* dataset, const char* name);
int dataset_remove_data(struct dataset* dataset, void* data);
int dataset_get_data(struct dataset* dataset, const char* name, void** data_ret);
void* dataset_get(struct dataset* dataset, const char* name);
int dataset_get_value(struct dataset* dataset, const char* name, long* value_ret);
const char* dataset_get_name(struct dataset* dataset, void* data);
int dataset_has_name(struct dataset* dataset, const char* name);
int dataset_copy(struct dataset* dest, const struct dataset* src);

struct dataset_iterator* dataset_first(struct dataset* dataset);
struct dataset_iterator* dataset_last(struct dataset* dataset);
struct dataset_iterator* dataset_next(struct dataset_iterator* it);
const char* dataset_itname(struct dataset_iterator* it);
void* dataset_itdata(struct dataset_iterator* it);
int dataset_itvalue(struct dataset_iterator* it);

TYPEDECL(dataset);

#endif
