#ifndef _STRING_LIST_H
#define _STRING_LIST_H

#include "datalist.h"

struct stringlist {
    struct datalist list;
};

static inline struct stringlist* create_stringlist()
{
    return (struct stringlist*)create_datalist();
}

#ifdef MEMDEBUG
#define stringlist_init(strlst) datalist_init(&(strlst)->list)
#else
static inline void stringlist_init(struct stringlist* stringlist)
{
    return datalist_init(&stringlist->list);
}
#endif

static inline void stringlist_ref(struct stringlist* stringlist)
{
    datalist_ref(&stringlist->list);
}

static inline void stringlist_free(struct stringlist* stringlist)
{
    datalist_free((struct datalist*)stringlist);
}

static inline int stringlist_size(struct stringlist* stringlist)
{
    return datalist_size(&stringlist->list);
}

int stringlist_insert(struct stringlist* stringlist, int index, const char* data);
int stringlist_append(struct stringlist* stringlist, const char* data);
int stringlist_remove(struct stringlist* stringlist, const char* data);
int stringlist_indexof(struct stringlist* stringlist, const char* data);
char* stringlist_getdata(struct stringlist* stringlist, int index);
int stringlist_copy(struct stringlist* dest, struct stringlist* src);

static inline struct stringlist_iterator* stringlist_first(struct stringlist* stringlist)
{
    return (struct stringlist_iterator*)datalist_first(&stringlist->list);
}

static inline struct stringlist_iterator* stringlist_last(struct stringlist* stringlist)
{
    return (struct stringlist_iterator*)datalist_last(&stringlist->list);
}

static inline struct stringlist_iterator* stringlist_next(struct stringlist_iterator* it)
{
    return (struct stringlist_iterator*)datalist_next((struct datalist_iterator*)it);
}

static inline char* stringlist_itdata(struct stringlist_iterator* it)
{
    return datalist_itdata((struct datalist_iterator*)it);
}

TYPEDECL(stringlist);

#endif
