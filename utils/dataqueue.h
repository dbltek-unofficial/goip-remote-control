#ifndef _DATAQUEUE_H
#define _DATAQUEUE_H

#include "utils.h"

struct dataqueue {
    int n;
    struct dataqueue_iterator* first;
    struct dataqueue_iterator* last;
};

struct dataqueue* create_dataqueue();
struct dataqueue* create_dataqueue_from(struct memblk**);
void dataqueue_init(struct dataqueue* pdataqueue);
void dataqueue_free(struct dataqueue* pdataqueue);

int dataqueue_size(struct dataqueue* pdataqueue);
int dataqueue_append(struct dataqueue* pdataqueue, void* data);
int dataqueue_next(struct dataqueue* pdataqueue, ... /* 'data_type **' */);
void* dataqueue_getnext(struct dataqueue* pdataqueue);
int dataqueue_peek(struct dataqueue* pdataqueue, ... /* 'data_type **' */);
int dataqueue_empty(struct dataqueue* pdataqueue);
int dataqueue_is_empty(struct dataqueue* pdataqueue);
int dataqueue_search(struct dataqueue* pdataqueue, void* data);
int dataqueue_remove(struct dataqueue* pdataqueue, void* data);
struct dataqueue_iterator* dataqueue_iterator_first(struct dataqueue* pdataqueue);
struct dataqueue_iterator* dataqueue_iterator_last(struct dataqueue* pdataqueue);
struct dataqueue_iterator* dataqueue_iterator_next(struct dataqueue_iterator* it);
void* dataqueue_iterator_getdata(struct dataqueue_iterator* it);

#endif

/* vim:ts=4:sw=4:
 */
