#include "event.h"
#include <stddef.h>
#include <string.h>

int event_set(struct event* event, int type, const void* buf)
{
    switch (type) {
    case EVP_CALLBACK:
        memcpy(&event->callback, buf, sizeof(event->callback));
        break;
    case EVP_ARG:
        memcpy(&event->arg, buf, sizeof(event->arg));
        break;
    default:
        return event->source->ops->set(event->source, event, type, buf);
    }

    return 0;
}

int event_get(struct event* event, int type, void* buf)
{
    switch (type) {
    case EVP_CALLBACK:
        memcpy(buf, &event->callback, sizeof(event->callback));
        break;
    case EVP_ARG:
        memcpy(buf, &event->arg, sizeof(event->arg));
        break;
    default:
        return event->source->ops->get(event->source, event, type, buf);
    }

    return 0;
}

int event_add(struct event* event)
{
    if (!event->callback)
        return -1;

    return event->source->ops->add(event->source, event);
}

void event_del(struct event* event)
{
    event->source->ops->del(event->source, event);
}

int event_loop_init(struct event_loop* loop)
{
    memset(loop, 0, sizeof(struct event_loop));
    INIT_LIST_HEAD(&loop->sources);
    INIT_LIST_HEAD(&loop->pending);
    return 0;
}

int event_loop_add_source(struct event_loop* loop, struct event_source* source, int primary)
{
    source->loop = loop;
    __list_add(&source->link, &loop->sources);
    if (primary) {
        loop->primary = source;
    }

    return 0;
}

int event_loop_del_source(struct event_loop* loop, struct event_source* source)
{
    return -1;
}

struct event* event_loop_new_event(struct event_loop* loop, unsigned events)
{
    struct list_head* lp;

    list_for_each(lp, &loop->sources)
    {
        struct event_source* source = __list_entry(lp, struct event_source, link);
        struct event* event;

        event = source->ops->new_event(source, events);
        if (event)
            return event;
    }

    return NULL;
}

void event_schedule(struct event_loop* loop, struct event* event)
{
    struct list_head* p;

    __list_del_init(&event->active_link);

    list_for_each(p, &loop->pending)
    {
        struct event* current;

        current = __list_entry(p, struct event, active_link);
        if (current->priority > event->priority) {
            break;
        }
    }

    __list_add_tail(&event->active_link, p);
}

int event_loop(struct event_loop* loop)
{
    if (__list_empty(&loop->sources) || !loop->primary) {
        return -1;
    }

    while (!loop->exit_status) {
        struct list_head* lp;
        int timeout = -1;

        list_for_each(lp, &loop->sources)
        {
            struct event_source* source = __list_entry(lp, struct event_source, link);
            source->ops->dispatch(source);
        }

        while (!__list_empty(&loop->pending)) {
            struct event* event;

            event = __list_entry(loop->pending.next, struct event, active_link);
            __list_del_init(loop->pending.next);

            event->source->ops->exec(event->source, event);
        }

        list_for_each(lp, &loop->sources)
        {
            struct event_source* source = __list_entry(lp, struct event_source, link);
            int t = source->ops->pending(source);

            //printf(" pending=%d\n", t);
            if (t >= 0 && (timeout < 0 || t < timeout)) {
                timeout = t;
                //printf(" timeout=%d\n", timeout);
            }
        }

        printf("wait: %d\n", timeout);
        loop->primary->ops->wait(loop->primary, timeout);
    }

    return (loop->exit_status & 0xff);
}

void event_loop_exit(struct event_loop* loop, int code)
{
    loop->exit_status = 0x100 | (code & 0xff);
}
