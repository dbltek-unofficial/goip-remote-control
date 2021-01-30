#include "dataset.h"

#include <string.h>

#include "debug.h"

struct dataset_iterator {
    const char* name;
    const struct typeinfo* typeinfo;
    union {
        void* data;
        long value;
    } u;
    struct dataset_iterator* next;
};

struct dataset* create_dataset()
{
    struct memblk* memblk = 0;
    struct dataset* dataset;

    if (!(dataset = memblk_alloc(&memblk, sizeof(struct dataset)))) {
        return 0;
    }
    bzero(dataset, sizeof(*dataset));
    dataset->memblk = memblk;
    dataset->ref++;

    return dataset;
}

void dataset_ref(struct dataset* dataset)
{
    dataset->ref++;
}

void dataset_init(struct dataset* dataset)
{
    /*	
	if(dataset->ref==1){
		DPRINT("%s: %d: %s(): dataset already initialized!\n", file, lineno, func);
		abort();
	}
*/
    bzero(dataset, sizeof(*dataset));
    dataset->ref++;
}

void dataset_free(struct dataset* dataset)
{
    struct dataset_iterator* it;

    if (dataset->ref <= 1) {
        struct memblk* memblk = dataset->memblk;
        for (it = dataset_first(dataset); it; it = dataset_next(it)) {
            if (it->typeinfo)
                type_free(it->typeinfo, it->u.data);
        }

        memset(dataset, 0, sizeof(struct dataset));
        memblk_free(&memblk);
    } else {
        dataset->ref--;
    }
}

void* dataset_allocmem(struct dataset* dataset, size_t nbytes)
{
    return memblk_alloc(&dataset->memblk, nbytes);
}

static inline struct dataset_iterator* alloc_dataset_iterator(struct dataset* dataset)
{
    struct dataset_iterator* it;

    if (dataset->ref <= 0) {
        DPRINT("%s(): not initialized dataset!\n", __FUNCTION__);
        abort();
    }

    if (!dataset->free) {
        it = memblk_alloc(&dataset->memblk, sizeof(*it));
        bzero(it, sizeof(*it));
    } else {
        it = dataset->free;
        dataset->free = it->next;
        it->next = 0;
    }

    return it;
}

static inline void free_dataset_iterator(struct dataset* dataset, struct dataset_iterator* it)
{
    if (it->typeinfo) {
        type_free(it->typeinfo, it->u.data);
    }

    bzero(it, sizeof(*it));
    it->next = dataset->free;
    dataset->free = it;
}

static void dataset_insert_iterator(struct dataset* dataset, struct dataset_iterator* it)
{
    if (!dataset->last) {
        dataset->first = dataset->last = it;
    } else {
        dataset->last->next = it;
        dataset->last = it;
    }
}

static struct dataset_iterator* dataset_find_by_name(struct dataset* dataset, const char* name)
{
    struct dataset_iterator* it;

    it = dataset->first;
    while (it) {
        if (!strcmp(it->name, name)) {
            break;
        }
        it = it->next;
    }
    return it;
}

void* dataset_insert_type(struct dataset* dataset, const char* name, const struct typeinfo* ti)
{
    void* data;
    struct dataset_iterator* it;

    if ((it = dataset_find_by_name(dataset, name))) {
        if (strcmp(it->typeinfo->name, ti->name)) {
            return 0;
        } else {
            return it->u.data;
        }
    }

    if (!(data = type_create(ti))) {
        return 0;
    }

    if (!(it = alloc_dataset_iterator(dataset))) {
        type_free(ti, data);
        return 0;
    }
    it->name = name;
    it->u.data = data;
    it->typeinfo = ti;

    dataset_insert_iterator(dataset, it);
    return data;
}

const struct typeinfo* dataset_get_type(struct dataset* dataset, const char* name)
{
    struct dataset_iterator* it;

    it = dataset_find_by_name(dataset, name);
    if (!it)
        return 0;

    return it->typeinfo;
}

int dataset_insert_data(struct dataset* dataset, const char* name, void* data)
{
    struct dataset_iterator* it;

    if (!(it = dataset_find_by_name(dataset, name))) {
        if (!(it = alloc_dataset_iterator(dataset))) {
            return -1;
        }
        it->name = name;
        dataset_insert_iterator(dataset, it);
    }
    if (it->typeinfo)
        type_copy(it->typeinfo, it->u.data, data);
    else
        it->u.data = data;

    return 0;
}

int dataset_insert_value(struct dataset* dataset, const char* name, long value)
{
    struct dataset_iterator* it;

    if (!(it = dataset_find_by_name(dataset, name))) {
        if (!(it = alloc_dataset_iterator(dataset))) {
            return -1;
        }
        it->name = name;
        dataset_insert_iterator(dataset, it);
    }
    it->u.value = value;

    return 0;
}

int dataset_insert_string(struct dataset* dataset, const char* name, const char* str)
{
    char* p;
    struct dataset_iterator* it;

    if (!(it = dataset_find_by_name(dataset, name))) {
        if (!(it = alloc_dataset_iterator(dataset))) {
            return -1;
        }
        it->name = name;
        dataset_insert_iterator(dataset, it);
    }

    p = it->u.data;
    if (!p || strlen(p) < strlen(str)) {
        if (!(p = dataset_allocmem(dataset, strlen(str) + 1))) {
            return -1;
        }
        it->u.data = p;
    }
    strcpy(p, str);

    return 0;
}

int dataset_remove_data(struct dataset* dataset, void* data)
{
    struct dataset_iterator* it;

    if (!(it = dataset->first)) {
        return -1;
    }
    if (it->u.data == data) {
        dataset->first = it->next;
        if (it == dataset->last) {
            dataset->last = 0;
        }
        free_dataset_iterator(dataset, it);
        return 0;
    } else {
        while (it->next) {
            if (it->next->u.data == data) {
                struct dataset_iterator* tmp = it->next;
                it->next = tmp->next;
                if (tmp == dataset->last)
                    dataset->last = it;
                free_dataset_iterator(dataset, tmp);
                return 0;
            }
            it = it->next;
        }
    }

    return -1;
}

int dataset_remove(struct dataset* dataset, const char* name)
{
    struct dataset_iterator* it;

    if (!(it = dataset->first)) {
        return -1;
    }
    if (!strcmp(it->name, name)) {
        dataset->first = it->next;
        if (it == dataset->last) {
            dataset->last = 0;
        }
        free_dataset_iterator(dataset, it);
        return 0;
    } else {
        while (it->next) {
            if (!strcmp(it->next->name, name)) {
                struct dataset_iterator* tmp = it->next;
                it->next = tmp->next;
                if (tmp == dataset->last)
                    dataset->last = it;
                free_dataset_iterator(dataset, tmp);
                return 0;
            }
            it = it->next;
        }
    }

    return -1;
}

static struct dataset_iterator* dataset_find_by_data(struct dataset* dataset, void* data)
{
    struct dataset_iterator* it;

    it = dataset->first;
    while (it) {
        if (it->u.data == data) {
            break;
        }
        it = it->next;
    }
    return it;
}

int dataset_get_data(struct dataset* dataset, const char* name, void** data_ret)
{
    struct dataset_iterator* it;
    if (!(it = dataset_find_by_name(dataset, name))) {
        return -1;
    }
    *data_ret = it->u.data;
    return 0;
}

void* dataset_get(struct dataset* dataset, const char* name)
{
    struct dataset_iterator* it;
    if (!(it = dataset_find_by_name(dataset, name))) {
        return NULL;
    }
    return it->u.data;
}

int dataset_get_value(struct dataset* dataset, const char* name, long* value)
{
    struct dataset_iterator* it;
    if (!(it = dataset_find_by_name(dataset, name))) {
        return -1;
    }

    *value = it->u.value;
    return 0;
}

const char* dataset_get_name(struct dataset* dataset, void* data)
{
    struct dataset_iterator* it;

    if (!(it = dataset_find_by_data(dataset, data))) {
        return 0;
    }

    return it->name;
}

int dataset_has_name(struct dataset* dataset, const char* name)
{
    return (dataset_find_by_name(dataset, name) != 0);
}

int dataset_copy(struct dataset* dest, const struct dataset* src)
{
    struct dataset_iterator* it;
    int count = 0;

    if (dest->first) {
        it = dest->first;
        while (it) {
            struct dataset_iterator* current = it;
            it = it->next;

            free_dataset_iterator(dest, current);
        }
        dest->first = dest->last = 0;
    }

    it = src->first;
    while (it) {
        if (it->typeinfo) {
            dataset_insert_type(dest, it->name, it->typeinfo);
        }
        dataset_insert_data(dest, it->name, it->u.data);
        it = it->next;
        count++;
    }

    return count;
}

struct dataset_iterator* dataset_first(struct dataset* dataset)
{
    return dataset->first;
}

struct dataset_iterator* dataset_last(struct dataset* dataset)
{
    return dataset->last;
}

struct dataset_iterator* dataset_next(struct dataset_iterator* it)
{
    return it ? it->next : 0;
}

const char* dataset_itname(struct dataset_iterator* it)
{
    return it ? it->name : 0;
}

void* dataset_itdata(struct dataset_iterator* it)
{
    return it ? it->u.data : 0;
}

int dataset_itvalue(struct dataset_iterator* it)
{
    return it ? it->u.value : 0;
}

TYPEDEF(struct dataset, dataset, dataset_init, dataset_free, dataset_copy);

// vim:ts=4:sw=4:
