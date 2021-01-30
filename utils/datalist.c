#include "datalist.h"
#include "string.h"

#include "debug.h"

struct datalist_iterator {
    const struct typeinfo* typeinfo;
    void* data;
    struct datalist_iterator* next;
};

static inline struct datalist_iterator* alloc_datalist_iterator(struct datalist* datalist)
{
    struct datalist_iterator* it;

#if 0
	if(datalist->ref<=0){
		DPRINT("%s(): datalist not initialized!\n", __FUNCTION__);
		abort();
	}
#endif

    if (!datalist->free) {
        it = memblk_alloc(&datalist->memblk, sizeof(struct datalist_iterator));
        bzero(it, sizeof(struct datalist_iterator));
    } else {
        it = datalist->free;
        datalist->free = it->next;
        it->next = 0;
    }

    return it;
}

static inline void free_datalist_iterator(struct datalist* datalist, struct datalist_iterator* it)
{
    bzero(it, sizeof(struct datalist_iterator));
    it->next = datalist->free;
    datalist->free = it;
}

struct datalist* create_datalist()
{
    struct datalist* datalist;
    struct memblk* memblk = 0;

    if (!(datalist = memblk_alloc(&memblk, sizeof(struct datalist)))) {
        return 0;
    }
    bzero(datalist, sizeof(struct datalist));
    datalist->memblk = memblk;

    datalist->ref++;
    return datalist;
}

void datalist_ref(struct datalist* datalist)
{
    datalist->ref++;
}

void datalist_init(struct datalist* datalist)
{
    bzero(datalist, sizeof(*datalist));
    datalist->ref++;
}

void datalist_free(struct datalist* datalist)
{
    if (datalist->ref > 1) {
        datalist->ref--;
        DLOG("datalist ref > 1!!!!!\n");
        abort();
    } else {
        struct memblk* memblk = datalist->memblk;
        struct datalist_iterator* it;

        for (it = datalist_first(datalist); it; it = datalist_next(it)) {
            if (it->typeinfo)
                type_free(it->typeinfo, it->data);
        }

        memset(datalist, 0, sizeof(struct datalist));
        memblk_free(&memblk);
    }
}

void* datalist_allocmem(struct datalist* datalist, size_t nbytes)
{
    return memblk_alloc(&datalist->memblk, nbytes);
}

int datalist_size(struct datalist* datalist)
{
    return datalist->n;
}

int datalist_insert(struct datalist* datalist, int index, void* data) //code by David
{
    struct datalist_iterator* it;

    if (!(it = alloc_datalist_iterator(datalist))) {
        return -1;
    }

    it->data = data;

    if (index == 0) {
        it->next = datalist->first;
        datalist->first = it;

        if (!datalist->last) {
            datalist->last = it;
        }
    } else if (index >= datalist->n) {
        datalist->last->next = it;
        datalist->last = it;
    } else {
        struct datalist_iterator* tmp = datalist->first;
        int i;
        for (i = 0; i < datalist->n; i++) {
            if (i == index) {
                break;
            }
            tmp = tmp->next;
        }

        it->next = tmp;
        tmp = it;
    }

    datalist->n++;

    return 0;
}

void* datalist_insert_type(struct datalist* datalist, int index, const struct typeinfo* ti)
{
    void* data;
    struct datalist_iterator* it;

    if (!(data = type_create(ti))) {
        return 0;
    }

    if (!(it = alloc_datalist_iterator(datalist))) {
        type_free(ti, data);
        return 0;
    }

    it->data = data;
    it->typeinfo = ti;

    if (index == 0) {
        it->next = datalist->first;
        datalist->first = it;

        if (!datalist->last) {
            datalist->last = it;
        }
    } else if (index >= datalist->n) {
        datalist->last->next = it;
        datalist->last = it;
    } else {
        struct datalist_iterator* tmp = datalist->first;
        int i;
        for (i = 0; i < datalist->n; i++) {
            if (i == index) {
                break;
            }
            tmp = tmp->next;
        }

        it->next = tmp;
        tmp = it;
    }

    datalist->n++;

    return data;
}

int datalist_append(struct datalist* datalist, void* data)
{
    struct datalist_iterator* it;

    if (!(it = alloc_datalist_iterator(datalist))) {
        return -1;
    }
    it->data = data;

    if (!datalist->last) {
        datalist->first = datalist->last = it;
    } else {
        datalist->last->next = it;
        datalist->last = it;
    }
    datalist->n++;

    return 0;
}

void* datalist_append_type(struct datalist* datalist, const struct typeinfo* ti)
{
    void* data;
    struct datalist_iterator* it;

    if (!(data = type_create(ti))) {
        return 0;
    }

    if (!(it = alloc_datalist_iterator(datalist))) {
        type_free(ti, data);
        return 0;
    }
    it->data = data;
    it->typeinfo = ti;

    if (!datalist->last) {
        datalist->first = datalist->last = it;
    } else {
        datalist->last->next = it;
        datalist->last = it;
    }
    datalist->n++;

    return data;
}

int datalist_remove_data(struct datalist* datalist, void* data)
{
    struct datalist_iterator* it;

    if (!(it = datalist->first)) {
        return -1;
    }
    if (it->data == data) {
        datalist->first = it->next;
        if (it == datalist->last) {
            datalist->last = 0;
        }
        free_datalist_iterator(datalist, it);
        datalist->n--;
        return 0;
    } else {
        while (it->next) {
            if (it->next->data == data) {
                struct datalist_iterator* tmp = it->next;
                it->next = tmp->next;
                if (tmp == datalist->last)
                    datalist->last = it;
                free_datalist_iterator(datalist, tmp);
                datalist->n--;
                return 0;
            }
            it = it->next;
        }
    }

    return -1;
}

int datalist_remove(struct datalist* datalist, int index) //code by David
{
    struct datalist_iterator *it, *tmp;

    if (index >= datalist->n || index < 0) {
        return -1;
    }

    if (!(it = datalist->first)) {
        return -1;
    }

    tmp = it->next;

    if (index == 0) {
        datalist->first->next = tmp->next;
        datalist->first = tmp;
        free_datalist_iterator(datalist, it);
        datalist->n--;

        return 0;
    } else {
        int i;
        for (i = 1; i < datalist->n; i++) {
            if (i == index) {
                it->next = tmp->next;
                if (tmp == datalist->last) {
                    datalist->last = it;
                }
                free_datalist_iterator(datalist, tmp);
                datalist->n--;
                return 0;
            }
            it = tmp;
            tmp = it->next;
        }
    }

    return -1;
}

int datalist_getdata(struct datalist* datalist, int index, void** data)
{
    struct datalist_iterator* it;
    int i;

    if (index >= datalist->n || index < 0) {
        return -1;
    }

    if (!(it = datalist->first)) {
        return -1;
    }

    for (i = 0; i < datalist->n; i++) {
        if (i == index) {
            *data = it->data;
            return 0;
        }

        it = it->next;
    }

    return -1;
}

void* datalist_get(struct datalist* datalist, int index)
{
    struct datalist_iterator* it;
    int i;

    if (index >= datalist->n || index < 0) {
        return NULL;
    }

    if (!(it = datalist->first)) {
        return NULL;
    }

    for (i = 0; i < datalist->n; i++) {
        if (i == index) {
            return it->data;
        }

        it = it->next;
    }

    return NULL;
}

struct datalist_iterator* datalist_first(struct datalist* datalist)
{
    return datalist->first;
}

struct datalist_iterator* datalist_last(struct datalist* datalist)
{
    return datalist->last;
}

struct datalist_iterator* datalist_next(struct datalist_iterator* it)
{
    return it ? it->next : 0;
}

void* datalist_itdata(struct datalist_iterator* it)
{
    return it ? it->data : 0;
}

int datalist_copy(struct datalist* dest, struct datalist* src)
{
    struct datalist_iterator* it;

    if (dest->first) {
        it = dest->first;
        while (it) {
            struct datalist_iterator* current = it;
            it = it->next;

            if (current->typeinfo) {
                type_free(current->typeinfo, current->data);
            }
            free_datalist_iterator(dest, current);
        }
        dest->first = dest->last = 0;
    }

    for (it = src->first; it; it = it->next) {
        if (it->typeinfo) {
            void* data;

            if (!(data = datalist_append_type(dest, it->typeinfo))) {
                return -1;
            }
            if (type_copy(it->typeinfo, data, it->data) < 0) {
                return -1;
            }
        } else {
            datalist_append(dest, it->data);
        }
    }

    return dest->n;
}

TYPEDEF(struct datalist, datalist, datalist_init, datalist_free, datalist_copy);

/* vim:ts=4:sw=4:
 */
