#ifndef __LIST_H
#define __LIST_H

/*
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole __lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) \
    {                        \
        &(name), &(name)     \
    }

#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr)  \
    do {                     \
        (ptr)->next = (ptr); \
        (ptr)->prev = (ptr); \
    } while (0)

/*
 * Insert a new entry between two known consecutive entries. 
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static __inline__ void __list_insert(struct list_head* newNode,
    struct list_head* prevNode,
    struct list_head* nextNode)
{
    nextNode->prev = newNode;
    newNode->next = nextNode;
    newNode->prev = prevNode;
    prevNode->next = newNode;
}

/**
 * __list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static __inline__ void __list_add(struct list_head* new, struct list_head* head)
{
    __list_insert(new, head, head->next);
}

/**
 * __list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static __inline__ void __list_add_tail(struct list_head* new, struct list_head* head)
{
    __list_insert(new, head->prev, head);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static __inline__ void __list_jion(struct list_head* prev,
    struct list_head* next)
{
    next->prev = prev;
    prev->next = next;
}

/**
 * __list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: __list_empty on entry does not return true after this, the entry is in an undefined state.
 */
static __inline__ void __list_del(struct list_head* entry)
{
    __list_jion(entry->prev, entry->next);
    entry->next = entry->prev = 0;
}

/**
 * __list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
static __inline__ void __list_del_init(struct list_head* entry)
{
    __list_jion(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

/**
 * __list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static __inline__ int __list_empty(struct list_head* head)
{
    return head->next == head;
}

/**
 * __list_splice - join two lists
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
static __inline__ void __list_splice(struct list_head* list, struct list_head* head)
{
    struct list_head* first = list->next;

    if (first != list) {
        struct list_head* last = list->prev;
        struct list_head* at = head->next;

        first->prev = head;
        head->next = first;

        last->next = at;
        at->prev = last;
    }
}

/**
 * __list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define __list_entry(ptr, type, member) \
    ((type*)((char*)(ptr) - (unsigned long)(&((type*)0)->member)))

/**
 * list_for_each -	iterate over a list
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head)            \
    for (pos = (head)->next; pos != (head); \
         pos = pos->next)

#define list_for_each_back(pos, head)       \
    for (pos = (head)->prev; pos != (head); \
         pos = pos->prev)

/**
 * list_for_each_safe	-	iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop counter.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_safe(pos, n, head)                   \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

#define list_for_each_safe_back(pos, n, head)              \
    for (pos = (head)->prev, n = pos->prev; pos != (head); \
         pos = n, n = pos->prev)

#endif
