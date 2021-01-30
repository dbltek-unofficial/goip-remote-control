#ifndef __COBJ__H
#define __COBJ__H

#include "observer.h"

#define __COBJ(_class)                \
    const struct _class##_ops* __ops; \
    struct _class##_sigtbl* __sigtbl; \
    struct cobj* __owner;             \
    struct list_head __children;      \
    struct list_head __sibling;       \
    struct list_head __slots;

struct cobj {
    __COBJ(cobj)
};

#define __COBJ_OPS(_class)        \
    void (*ctor)(struct _class*); \
    void (*dtor)(struct _class*); \
    void (*release)(struct _class*);

struct cobj_ops {
    __COBJ_OPS(cobj)
};

static inline void cobj_release(struct cobj* obj)
{
    obj->__ops->release(obj);
}

struct cobj_sigtbl {
    int size;
    struct __signal signals[1];
};

struct cobj_nosigtbl {
    int size;
};

struct cobj* __cobj_new(int obj_size, const struct cobj_ops* ops, int sigtbl_size, struct cobj* owner);
#define cobj_new(type, ops, sigtbl, owner) ((type*)__cobj_new(sizeof(type), (struct cobj_ops*)ops, (sizeof(sigtbl) - sizeof(int)) / sizeof(struct __signal), owner))

void __cobj_detach(struct cobj*);

void __cobj_del(struct cobj*);
#define cobj_del(obj) __cobj_del((struct cobj*)(obj))

struct cobj_slot {
    struct list_head link;
    struct __slot slot;
    struct cobj* obj;
    void (*dtor)(struct cobj_slot*);
};

#define cobj_signal(obj, name) (&(obj)->__sigtbl->name)

void __cobj_signal_connect(struct __signal* sig, struct cobj* obj, struct cobj_slot* slot);
#define cobj_observer(signal, sobj, slot, cb)                                          \
    do {                                                                               \
        __cobj_signal_connect((struct __signal*)(signal), (struct cobj*)sobj, (slot)); \
        (signal)->prev->callback = cb;                                                 \
    } while (0)

#endif
