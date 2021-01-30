#include "btrace.h"

#if defined(__arm__)

struct layout {
    struct layout* next;
    void* sp;
    void* return_address;
};

int btrace_from(void* fp, void* sp, void** array, int size)
{
    struct layout* current;

    int cnt = 0;

    /* We skip the call to this function, it makes no sense to record it.  */
    current = (((struct layout*)fp) - 1);
    while (cnt < size) {
        if ((void*)current < sp)
            /* 
			 * This means the address is out of range.  Note that for the
			 * toplevel we see a frame pointer with value NULL which clearly is
			 * out of range.
			 */
            break;

        array[cnt++] = current->return_address;

        if (current->next == 0 || current->next < current || ((char*)current->next) - ((char*)current) > 0x100000UL)
            break;

        current = (current->next - 1);
    }

    return cnt;
}

int btrace(void** array, int size)
{
    register void* fp __asm__("fp");
    register void* sp __asm__("sp");

    return btrace_from((((struct layout*)fp) - 1)->next, sp, array, size);
}

#elif defined(__i386__)

struct layout {
    struct layout* next;
    void* return_address;
};

int btrace_from(void* ebp, void* esp, void** array, int size)
{
    struct layout* current;
    int cnt = 0;

    current = (struct layout*)ebp;
    while (cnt < size) {
        if ((void*)current < esp)
            /* 
			 * This means the address is out of range.  Note that for the
			 * toplevel we see a frame pointer with value NULL which clearly is
			 * out of range.
			 */
            break;

        array[cnt++] = current->return_address;

        if (current->next == 0 || current->next < current || ((char*)current->next) - ((char*)current) > 0x100000UL)
            break;

        current = current->next;
    }

    return cnt;
}

int btrace(void** array, int size)
{
    /* We assume that all the code is generated with frame pointers set.  */
    register void* ebp __asm__("ebp");
    register void* esp __asm__("esp");

    return btrace_from(((struct layout*)ebp)->next, esp, array, size);
}

#else

#error "btrace: arch not supported!"

#endif

int btrace_init()
{
    return 0;
}

int __btrace_from(void* fp, void* sp, void** array, int size)
{
    return btrace_from(fp, sp, array, size);
}
