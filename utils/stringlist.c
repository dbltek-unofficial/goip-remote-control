#include "stringlist.h"

int stringlist_insert(struct stringlist* stringlist, int index, const char* data)
{
    int slen;
    char* p;

    slen = strlen(data) + 1;
    if (!(p = datalist_allocmem(&stringlist->list, slen))) {
        return -1;
    }
    strcpy(p, data);

    return datalist_insert(&stringlist->list, index, p);
}

int stringlist_append(struct stringlist* stringlist, const char* data)
{
    int slen;
    char* p;

    slen = strlen(data) + 1;
    if (!(p = datalist_allocmem((struct datalist*)stringlist, slen))) {
        return -1;
    }
    strcpy(p, data);

    return datalist_append(&stringlist->list, p);
}

int stringlist_remove(struct stringlist* stringlist, const char* data)
{
    struct datalist_iterator* it;

    for (it = datalist_first(&stringlist->list); it; it = datalist_next(it)) {
        char* p = datalist_itdata(it);

        if (!strcmp(data, p)) {
            return datalist_remove_data(&stringlist->list, p);
        }
    }

    return -1;
}

char* stringlist_getdata(struct stringlist* stringlist, int index)
{
    void* data;

    if (datalist_getdata(&stringlist->list, index, &data) < 0) {
        return 0;
    }

    return data;
}

int stringlist_indexof(struct stringlist* stringlist, const char* data)
{
    struct datalist_iterator* it;
    int index;

    for (index = 0, it = datalist_first(&stringlist->list); it; it = datalist_next(it), index++) {
        char* p = datalist_itdata(it);

        if (!strcmp(data, p)) {
            return index;
        }
    }

    return -1;
}

int stringlist_copy(struct stringlist* dest, struct stringlist* src)
{
    struct datalist_iterator* it;
    int i;

    //	stringlist_free(dest);
    //	stringlist_init(dest);

    for (it = datalist_first(&src->list), i = 0; it; it = datalist_next(it), i++) {
        char* p = datalist_itdata(it);

        stringlist_append(dest, p);
    }

    return i;
}

void __stringlist_init(struct stringlist* stringlist)
{
    datalist_init((struct datalist*)stringlist);
}

TYPEDEF(struct stringlist, stringlist, __stringlist_init, datalist_free, stringlist_copy);
