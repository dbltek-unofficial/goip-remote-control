#ifndef __OBSERVER_H
#define __OBSERVER_H

#include "list.h"

#define SIGNAL_ARGMAX 8

struct __slot;

struct __signal {
    struct list_head slots; /* slot list */
    unsigned long flags;
    int (*emit)(struct __signal*, ...);
};

/* Initialize signal structure */
extern void __signal_init(struct __signal*);

/* Clear slots list in signal */
extern void __signal_fini(struct __signal*);

/* Observe signal with slot, add slot to slots list of signal */
extern void __signal_connect(struct __signal*, struct __slot*);

/* Emit signal, execute callback of all slot in slots list */
extern int __signal_emit(struct __signal*, ...);

#define SIGNAL(args...)                        \
    struct                                     \
    {                                          \
        SLOT(args) * next, *prev;              \
        unsigned long flags;                   \
        int (*emit)(struct __signal*, ##args); \
    }

#define SIGNAL_INIT(name)                                        \
    {                                                            \
        (void*)&(name), (void*)&(name), 0, (void*)&__signal_emit \
    }
#define INIT_SIGNAL(ptr)                     \
    do {                                     \
        (ptr)->next = (void*)(ptr);          \
        (ptr)->prev = (void*)(ptr);          \
        (ptr)->flags = 0;                    \
        (ptr)->emit = (void*)&__signal_emit; \
    } while (0)
#define SIGNAL_INIT_EX(name, flags)                                    \
    {                                                                  \
        (void*)&(name), (void*)&(name), (flags), (void*)&__signal_emit \
    }
#define INIT_SIGNAL_EX(ptr, flags)           \
    do {                                     \
        (ptr)->next = (void*)(ptr);          \
        (ptr)->prev = (void*)(ptr);          \
        (ptr)->flags = (flags);              \
        (ptr)->emit = (void*)&__signal_emit; \
    } while (0)
#define SIGNAL_EMIT(sig, args...) ((sig)->emit((struct __signal*)(sig), ##args))
#define SIGNAL_CONNECT(sig, slot)                                          \
    do {                                                                   \
        __signal_connect((struct __signal*)(sig), (struct __slot*)(slot)); \
        (sig)->prev->callback = (slot)->callback;                          \
    } while (0)

struct __slot {
    struct list_head link;
    int (*callback)(struct __slot*, ...);
};

#define SLOT(args...)                            \
    struct                                       \
    {                                            \
        struct list_head link;                   \
        int (*callback)(struct __slot*, ##args); \
    }
#define SLOT_INIT(name, cb)                \
    {                                      \
        { &(name).link, &(name).link }, cb \
    }
#define INIT_SLOT(ptr, cb)               \
    do {                                 \
        (ptr)->link.prev = &(ptr)->link; \
        (ptr)->link.next = &(ptr)->link; \
        (ptr)->callback = cb;            \
    } while (0)
#define SLOT_DISCONNECT(ptr) __slot_disconnect((struct __slot*)ptr)

void __slot_disconnect(struct __slot* slot);

#endif
