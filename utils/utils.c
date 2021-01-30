#include <string.h>

#include "debug.h"
#include "utils.h"

size_t strlen16bit(const unsigned short* s)
{
    size_t len = 0;
    while (*s++)
        len++;
    return len;
}

unsigned short* strcpy16bit(unsigned short* dest, const unsigned short* src)
{
    unsigned short* p = dest;
    while (*src)
        *p++ = *src++;
    *p = 0;

    return dest;
}

int strcmp16bit(unsigned short* s1, unsigned short* s2)
{
    while (*s1 && *s2) {
        if (*s1 != *s2)
            break;
        s1++;
        s2++;
    }

    return *s1 - *s2;
}

unsigned short* strdup16bit(const unsigned short* s)
{
    int len;
    unsigned short* r;
    len = strlen16bit(s);
    if (!(r = malloc((len + 1) * 2))) {
        return 0;
    }
    strcpy16bit(r, s);
    return r;
}

int strto16bit(unsigned short* str16, const char* str)
{
    int count = 0;
    while (*str) {
        *str16++ = *str++;
        count++;
    }
    *str16 = 0;

    return count;
}

int qnstrcmp(const char* src, size_t src_len, const char* dest, size_t dest_len)
{
    if (src_len != dest_len) {
        return (src_len - dest_len);
    }

    if (!src_len) {
        return -1;
    }

    if (src[0] == dest[0] && src[src_len - 1] == dest[src_len - 1]) {
        if (src_len > 2) {
            if (src[src_len / 2] == dest[src_len / 2]) {
                return strncmp(src + 1, dest + 1, src_len - 2);
            }
        } else
            return 0;
    }

    return -1;
}

#define MEMBLK_HDRSIZE (sizeof(unsigned) + sizeof(struct memblk*))
#define MAX_MEMBLKSIZE ((64 * 1024) - MEMBLK_HDRSIZE)

static inline size_t calc_blksize(size_t nbytes)
{
    int m = 12;

    nbytes >>= m;
    while (nbytes) {
        nbytes >>= 1;
        m++;
    }

    return ((size_t)(1 << m));
}

void* memblk_alloc(struct memblk** ppmemblk, size_t nbytes)
{
    struct memblk* ptr;
    void* res;

    if (!ppmemblk) {
        return 0;
    }

    ptr = *ppmemblk;
    nbytes = ((nbytes + 3) & (~3L));

    if (nbytes > MAX_MEMBLKSIZE) {
        DLOG("requested block size exceeds the maximun limit(%d): %d", MAX_MEMBLKSIZE, nbytes);
        abort();
        return 0;
    }

    while (ptr && ptr->free < nbytes) {
        ptr = ptr->next;
    }

    if (ptr) {
        res = &ptr->data[ptr->size - (ptr->free)];
        ptr->free -= nbytes;
    } else {
        size_t sz;

        sz = calc_blksize(nbytes + MEMBLK_HDRSIZE);
        if (!(ptr = (struct memblk*)malloc(sz))) {
            return 0;
        }
        bzero(ptr, sz);
        ptr->next = *ppmemblk;
        *ppmemblk = ptr;
        ptr->size = sz - MEMBLK_HDRSIZE;
        ptr->free = ptr->size - nbytes;

        res = ptr->data;
    }

    return res;
}

void memblk_free(struct memblk** ppmemblk)
{
    struct memblk* p = *ppmemblk;
    int total = 0;

    *ppmemblk = 0;
    while (p) {
        struct memblk* tmp;
        tmp = p;
        p = tmp->next;
        total += tmp->size;
        //#ifdef MEMDEBUG
        //        __memdebug_free(tmp, __FILE__, __LINE__, __FUNCTION__, "free(tmp)");
        //#else
        free(tmp);
        //#endif
    }
}

unsigned r5_hash(const signed char* msg, int len)
{
    unsigned a = 0;
    while (*msg && len--) {
        a += *msg << 4;
        a += *msg >> 4;
        a *= 11;
        msg++;
    }
    return a;
}

/* vim:ts=4:sw=4:tw=78:set et: */
