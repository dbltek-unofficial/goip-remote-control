#include "dataqueue.h"

#include <stdarg.h>
#include <string.h>

struct dataqueue_iterator {
    void* data;
    struct dataqueue_iterator* next;
};

static inline struct dataqueue_iterator* alloc_dataqueue_iterator(struct dataqueue* pdataqueue)
{
    struct dataqueue_iterator* it;

    it = malloc(sizeof(*it));
    bzero(it, sizeof(*it));

    return it;
}

static inline void free_dataqueue_iterator(struct dataqueue* pdataqueue, struct dataqueue_iterator* it)
{
    free(it);
}

struct dataqueue* create_dataqueue()
{
    struct dataqueue* pdataqueue;

    if (!(pdataqueue = malloc(sizeof(struct dataqueue)))) {
        return NULL;
    }

    bzero(pdataqueue, sizeof(*pdataqueue));

    return pdataqueue;
}

void dataqueue_init(struct dataqueue* pdataqueue)
{
    bzero(pdataqueue, sizeof(*pdataqueue));
}

void dataqueue_free(struct dataqueue* pdataqueue)
{
    dataqueue_empty(pdataqueue);
    free(pdataqueue);
}

int dataqueue_size(struct dataqueue* pdataqueue)
{
    return pdataqueue->n;
}

int dataqueue_append(struct dataqueue* pdataqueue, void* data)
{
    struct dataqueue_iterator* it;

    if (!(it = alloc_dataqueue_iterator(pdataqueue))) {
        return -1;
    }

    it->data = data;

    if (!pdataqueue->last) {
        pdataqueue->first = pdataqueue->last = it;
    } else {
        pdataqueue->last->next = it;
        pdataqueue->last = it;
    }

    pdataqueue->n++;

    return 0;
}

int dataqueue_next(struct dataqueue* pdataqueue, ...)
{
    void** pdata;
    va_list ap;

    va_start(ap, pdataqueue);
    pdata = va_arg(ap, void**);

    if (pdataqueue->n > 0) {
        struct dataqueue_iterator* it;

        if (!(it = pdataqueue->first)) {
            return -1;
        }
        *pdata = it->data;

        if (it == pdataqueue->last) {
            pdataqueue->first = pdataqueue->last = 0;
        } else {
            pdataqueue->first = it->next;
        }

        free_dataqueue_iterator(pdataqueue, it);
        pdataqueue->n--;

        return 0;
    }

    return -1;
}

void* dataqueue_getnext(struct dataqueue* pdataqueue)
{
    void* data = NULL;

    if (pdataqueue->n > 0) {
        struct dataqueue_iterator* it;

        if (!(it = pdataqueue->first)) {
            return NULL;
        }
        data = it->data;

        if (it == pdataqueue->last) {
            pdataqueue->first = pdataqueue->last = 0;
        } else {
            pdataqueue->first = it->next;
        }

        free_dataqueue_iterator(pdataqueue, it);
        pdataqueue->n--;
    }

    return data;
}

int dataqueue_peek(struct dataqueue* pdataqueue, ...)
{
    void** pdata;
    va_list ap;

    va_start(ap, pdataqueue);
    pdata = va_arg(ap, void**);

    if (pdataqueue->n > 0) {
        struct dataqueue_iterator* it;

        if (!(it = pdataqueue->first)) {
            return -1;
        }

        *pdata = it->data;

        return 0;
    }

    return -1;
}

int dataqueue_is_empty(struct dataqueue* pdataqueue)
{
    return (pdataqueue->n == 0);
}

int dataqueue_empty(struct dataqueue* pdataqueue)
{
    if (pdataqueue->n > 0) {
        struct dataqueue_iterator *it, *tmp;

        if (!(it = pdataqueue->first)) {
            return 0;
        }

        while (it) {
            tmp = it->next;
            free_dataqueue_iterator(pdataqueue, it);
            pdataqueue->n--;
            it = tmp;
        }
    }
    pdataqueue->first = 0;

    return 0;
}

int dataqueue_search(struct dataqueue* pdataqueue, void* data)
{
    if (!dataqueue_is_empty(pdataqueue)) {
        struct dataqueue_iterator* it;
        int i = 1;
        if (!(it = pdataqueue->first)) {
            return -1;
        }

        while (it) {
            if (it->data == data) {
                return i;
            }
            it = it->next;
            i++;
        }
    }

    return -1;
}

int dataqueue_remove(struct dataqueue* pdataqueue, void* data)
{
    if (pdataqueue->n <= 0)
        return -1;

    else {
        struct dataqueue_iterator *it = pdataqueue->first, *tmp = it;
        while (it) {
            if (it->data == data) {
                if (it == pdataqueue->first) {
                    if (pdataqueue->first == pdataqueue->last) {
                        pdataqueue->first = pdataqueue->last = NULL;
                    } else {
                        pdataqueue->first = it->next;
                    }
                } else if (it == pdataqueue->last) {
                    pdataqueue->last = tmp;
                    tmp->next = NULL;
                } else
                    tmp->next = it->next;
                free_dataqueue_iterator(pdataqueue, it);
                pdataqueue->n--;
                return 0;
            }
            tmp = it;
            it = it->next;
        }
        return -1;
    }
}

struct dataqueue_iterator* dataqueue_iterator_first(struct dataqueue* pdataqueue)
{
    return pdataqueue->first;
}

struct dataqueue_iterator* dataqueue_iterator_last(struct dataqueue* pdataqueue)
{
    return pdataqueue->last;
}

struct dataqueue_iterator* dataqueue_iterator_next(struct dataqueue_iterator* it)
{
    return it ? it->next : 0;
}

void* dataqueue_iterator_getdata(struct dataqueue_iterator* it)
{
    return it ? it->data : 0;
}

/* vim:ts=4:sw=4:
*/
