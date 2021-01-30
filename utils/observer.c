#include "observer.h"
#include <stdarg.h>
#include <string.h>

int __signal_emit(struct __signal* signal, ...)
{
    va_list ap;
    void* args[SIGNAL_ARGMAX];
    struct list_head* p;
    int i;

    va_start(ap, signal);
    for (i = 0; i < SIGNAL_ARGMAX; i++) {
        args[i] = va_arg(ap, void*);
    }

    i = 0;
    list_for_each(p, &signal->slots)
    {
        struct __slot* slot;
        int res;

        slot = __list_entry(p, struct __slot, link);
        res = slot->callback(slot, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
    }

    return i;
}

void __signal_init(struct __signal* sig)
{
    memset(sig, 0, sizeof(struct __signal));
    INIT_LIST_HEAD(&sig->slots);
    sig->emit = __signal_emit;
}

void __signal_fini(struct __signal* sig)
{
    while (!__list_empty(&sig->slots)) {
        __list_del_init(sig->slots.next);
    }
}

void __signal_connect(struct __signal* sig, struct __slot* slot)
{
    __list_add_tail(&slot->link, (struct list_head*)sig);
}

void __slot_disconnect(struct __slot* slot)
{
    __list_del_init(&slot->link);
}

#if 0
typedef SLOT(int a, char *b) test_slot_t;
typedef SIGNAL(int a, char *b) test_signal_t;
int test_cb(struct __slot *slot, int a, char *b)
{
	printf("test_cb(%d, \"%s\")\n", a, b);
	return 0;
}

int main()
{
	test_signal_t sig=SIGNAL_INIT(sig);
	test_slot_t slot=SLOT_INIT(slot, test_cb);
	//SLOT(int a, char *b) *p=&slot;
	
	INIT_SIGNAL(&sig);
	INIT_SLOT(&slot, test_cb);
	
	printf("test_cb=%p\n", &test_cb);
	printf("slot->callback=%p\n", slot.callback);

	SIGNAL_CONNECT(&sig, &slot);
	printf("sig->prev->callback=%p slot->callback=%p\n", sig.prev->callback, slot.callback);
	
	SIGNAL_EMIT(&sig, 1, "hello");

}

#endif
