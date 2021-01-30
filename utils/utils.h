#ifndef _UTILS_H
#define _UTILS_H

#include <stdlib.h>
#include <string.h>

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define items_of(data, type) (sizeof(data) / sizeof(type))
#define ARRAY_SIZE(data) (sizeof(data) / sizeof(data[0]))
#define OFFSET_OF(element, type) ((unsigned int)(&((type*)NULL)->element))

size_t strlen16bit(const unsigned short* s);
unsigned short* strcpy16bit(unsigned short* dest, const unsigned short* src);
unsigned short* strdup16bit(const unsigned short* s);
int strcmp16bit(unsigned short* s1, unsigned short* s2);
int strto16bit(unsigned short* str16, const char* str);
int unicode_to_utf8(const unsigned short* uc, int len, char* out, int len_out);
int utf8_to_unicode(const char* chars, int len, unsigned short* result, int len_out);

int qnstrcmp(const char* src, size_t src_len, const char* dest, size_t dest_len);

unsigned r5_hash(const signed char* msg, int len);

struct memblk {
    struct memblk* next;
    unsigned size;
    unsigned free;
    unsigned char data[1];
};

void* memblk_alloc(struct memblk** ppmemblk, size_t nbytes);
void memblk_free(struct memblk** ppmemblk);

struct dyndata {
    unsigned int len;
    void* data;
};

#endif
