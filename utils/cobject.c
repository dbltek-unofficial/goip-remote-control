#include "cobject.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct cobj* __cobj_new(int obj_size, const struct cobj_ops* ops, int sigtbl_size, struct cobj* owner)
{
    struct cobj* obj;

    obj = (struct cobj*)malloc(obj_size);
    memset(obj, 0, obj_size);
    obj->__ops = ops;
    if (sigtbl_size > 0) {
        int i;
        obj->__sigtbl = malloc(sizeof(struct cobj_sigtbl) + sizeof(struct __signal) * (sigtbl_size - 1));
        obj->__sigtbl->size = sigtbl_size;
        for (i = 0; i < sigtbl_size; i++) {
            __signal_init(&obj->__sigtbl->signals[i]);
        }
    }
    INIT_LIST_HEAD(&obj->__children);
    INIT_LIST_HEAD(&obj->__sibling);
    INIT_LIST_HEAD(&obj->__slots);

    if (owner) {
        obj->__owner = owner;
        __list_add_tail(&obj->__sibling, &owner->__children);
    }

    if (ops->ctor) {
        ops->ctor(obj);
    }

    return obj;
}

void __cobj_detach(struct cobj* obj)
{
    while (!__list_empty(&obj->__children)) {
        struct cobj* child;

        child = __list_entry(obj->__children.next, struct cobj, __sibling);
        __list_del_init(&child->__sibling);
        cobj_release(child);
    }

    while (!__list_empty(&obj->__slots)) {
        struct cobj_slot* slot;

        slot = __list_entry(obj->__slots.next, struct cobj_slot, link);
        __slot_disconnect(&slot->slot);
        __list_del_init(&slot->link);
        if (slot->dtor)
            slot->dtor(slot);
    }

    __list_del_init(&obj->__sibling);
    obj->__owner = NULL;

    if (obj->__sigtbl) {
        int i;

        for (i = 0; i < obj->__sigtbl->size; i++) {
            __signal_fini(&obj->__sigtbl->signals[i]);
        }
    }
}

void __cobj_del(struct cobj* obj)
{
    __cobj_detach(obj);
    free(obj);
}

void __cobj_slot_dtor(struct cobj_slot* slot)
{
    free(slot);
}

void __cobj_signal_connect(struct __signal* sig, struct cobj* obj, struct cobj_slot* slot)
{
    if (!slot) {
        slot = malloc(sizeof(struct cobj_slot));
        memset(slot, 0, sizeof(struct cobj_slot));
        slot->dtor = __cobj_slot_dtor;
    }
    __signal_connect(sig, &slot->slot);
    slot->obj = obj;
    __list_add(&slot->link, &obj->__slots);
}
