#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "event.h"
#include "poll.h"

#define SOURCE ((SOURCETYPE*)(source))
#define SOURCETYPE struct fdevent_source

#define EVENT ((EVENTTYPE*)(event))
#define EVENTTYPE struct fdevent

struct fdevent {
    struct event base;
    int fd;
    int id;
    unsigned events;
    unsigned revents;
    int timeoutval;
    struct timeval timeout;
    struct list_head timeout_link;
};

struct fdevent_source {
    struct event_source base;
    int fdsize;
    int fdcount;
    struct pollfd* pollfds;
    struct fdevent** fdevents;
    struct list_head timeout_list;
};

struct event* fdevent_new(struct event_source* source, int events)
{
    struct fdevent* fdevent;

    if ((events & (EV_READ | EV_WRITE)) == 0) {
        return NULL;
    }

    fdevent = malloc(sizeof(struct fdevent));
    memset(fdevent, 0, sizeof(struct fdevent));

    INIT_LIST_HEAD(&fdevent->base.active_link);
    fdevent->base.source = source;
    fdevent->events = events;
    fdevent->fd = -1;
    fdevent->id = -1;
    INIT_LIST_HEAD(&fdevent->timeout_link);

    return (struct event*)fdevent;
}

void add_to_timeout_list(struct fdevent_source* source, struct fdevent* event)
{
    struct list_head* p;

    //DLOG("event=%p", event);

    gettimeofday(&event->timeout, NULL);
    event->timeout.tv_sec += event->timeoutval / 1000;
    event->timeout.tv_usec += (event->timeoutval % 1000) * 1000;

    list_for_each(p, &SOURCE->timeout_list)
    {
        struct fdevent* current;

        current = __list_entry(p, struct fdevent, timeout_link);
        if (EVENT->timeout.tv_sec < current->timeout.tv_sec || (current->timeout.tv_sec == EVENT->timeout.tv_sec && current->timeout.tv_usec > EVENT->timeout.tv_usec)) {
            break;
        }
    }

    __list_add_tail(&EVENT->timeout_link, p);
}

int fdevent_set(struct event_source* source, struct event* event, int type, const void* buf)
{
    switch (type) {
    case EVP_FD:
        EVENT->fd = *((int*)buf);
        break;
    case EVP_EVENTS:
        memcpy(&EVENT->events, buf, sizeof(unsigned int));
        EVENT->events &= (EV_READ | EV_WRITE);
        if (EVENT->id >= 0) {
            SOURCE->pollfds[EVENT->id].events = 0;
            if (EVENT->events & EV_READ)
                SOURCE->pollfds[EVENT->id].events |= POLLIN;
            if (EVENT->events & EV_WRITE)
                SOURCE->pollfds[EVENT->id].events |= POLLOUT;
        }
        break;
    case EVP_TIMEOUT:
        memcpy(&EVENT->timeoutval, buf, sizeof(int));
        if (EVENT->id >= 0 && EVENT->timeoutval > 0) {
            add_to_timeout_list(SOURCE, EVENT);
        }
        break;
    default:
        return -1;
    }

    return 0;
}

int fdevent_get(struct event_source* source, struct event* event, int type, void* buf)
{
    switch (type) {
    case EVP_FD:
        memcpy(buf, &EVENT->fd, sizeof(int));
        break;
    case EVP_EVENTS:
        memcpy(buf, &EVENT->events, sizeof(unsigned int));
        break;
    case EVP_TIMEOUT:
        memcpy(&EVENT->timeoutval, buf, sizeof(int));
        break;
#if 0
		case XXX:
			if(EVENT->id >= 0 && EVENT->timeout.tv_sec > 0){
				struct timeval current;
				int t;

				gettimeofday(&current, NULL);
				t = (EVENT->timeout.tv_sec - current.tv_sec)*1000 + 
					(EVENT->timeout.tv_usec - current.tv_usec)/1000;
				if(t < 0 )t=0;
				*((int*)buf) = t;
			}
			else {
				*((int*)buf)=-1;
			}
			break;
#endif
    default:
        return -1;
    }

    return 0;
}

int fdevent_add(struct event_source* source, struct event* event)
{
    unsigned events = 0;

    //DLOG("add event: %p", event);
    if (EVENT->id >= 0 && EVENT->id < SOURCE->fdcount && SOURCE->fdevents[EVENT->id] == EVENT) {
        return 0;
    }

    if (EVENT->fd < 0 || SOURCE->fdcount >= SOURCE->fdsize) {
        return -1;
    }

    EVENT->id = SOURCE->fdcount;
    if (EVENT->events & EV_READ)
        events |= POLLIN;
    if (EVENT->events & EV_WRITE)
        events |= POLLOUT;
    SOURCE->pollfds[EVENT->id].fd = EVENT->fd;
    SOURCE->pollfds[EVENT->id].events = events;
    SOURCE->pollfds[EVENT->id].revents = 0;
    SOURCE->fdevents[SOURCE->fdcount] = EVENT;
    SOURCE->fdcount++;

    if (EVENT->timeoutval > 0) {
        add_to_timeout_list(SOURCE, EVENT);
    }

    return 0;
}

static void remove_event(struct fdevent_source* source, struct fdevent* event)
{
    if (event->id >= 0) {
        //DLOG("event=%p fd=%d fdcount=%d", event, event->fd, source->fdcount);
        source->fdcount--;
        if (event->id < source->fdcount) {
            source->pollfds[event->id] = source->pollfds[source->fdcount];
            source->fdevents[event->id] = source->fdevents[source->fdcount];
            source->fdevents[event->id]->id = event->id;
        }
        //else DLOG("the last fd!!!");
        event->id = -1;
        //DLOG("fdcount=%d", source->fdcount);
    } else {
        LOG_FUNCTION_FAILED;
    }

    __list_del_init(&event->timeout_link);
}

int fdevent_del(struct event_source* source, struct event* event)
{
    if (event->source != source || (EVENT->id >= 0 && (EVENT->id >= SOURCE->fdcount || EVENT != SOURCE->fdevents[EVENT->id]))) {
        LOG_FUNCTION_FAILED;
        return -1;
    }

    CHECKPOINT;
    remove_event(SOURCE, EVENT);
    __list_del(&event->active_link);

    free(event);

    return 0;
}

int fdevent_exec(struct event_source* source, struct event* event)
{
    if (event->source != source)
        return -1;
    EVENT->revents &= (EVENT->events | EV_ERROR);
    event->callback(event->arg, EVENT->revents, EVENT->fd);

    /*	
	if(EVENT->timeoutval){
		add_to_timeout_list(SOURCE, EVENT);
	}
*/

    return 0;
}

int fdevent_pending(struct event_source* source)
{
    struct fdevent* ev;
    struct timeval current;
    int res;

    if (__list_empty(&SOURCE->timeout_list))
        return -1;

    ev = __list_entry(SOURCE->timeout_list.next, struct fdevent, timeout_link);
    gettimeofday(&current, NULL);

    res = (ev->timeout.tv_usec - current.tv_usec) / 1000 + (ev->timeout.tv_sec - current.tv_sec) * 1000;

    if (res < 0)
        res = 0;

    return res;
}

int fdevent_wait(struct event_source* source, int timeout)
{
    int res;

    //DLOG("timeout=%d fdcount=%d", timeout, SOURCE->fdcount);
    res = poll(SOURCE->pollfds, SOURCE->fdcount, timeout);
    //DLOG("poll return %d", res);
#if 0
	if(res < 0){
		if(errno != EINTR && errno != EAGAIN)
			return -1;
	}
#endif
#if 0
	if(getenv("DEBUG") && strcmp(getenv("DEBUG"), "1")==0){
		int i;
		for(i=0; i<SOURCE->fdcount; i++){
			DLOG("   %d", SOURCE->pollfds[i].fd);
		}
	}
#endif

    return 0;
}

int fdevent_dispatch(struct event_source* source)
{
    struct timeval current;
    int i;

    for (i = 0; i < SOURCE->fdcount; i++) {
        //DLOG("fdevents[%d]=%p", i, SOURCE->fdevents[i]);
        if (SOURCE->pollfds[i].revents) {
            SOURCE->fdevents[i]->revents = 0;
            if (SOURCE->pollfds[i].revents & POLLIN)
                SOURCE->fdevents[i]->revents |= EV_READ;
            if (SOURCE->pollfds[i].revents & POLLOUT)
                SOURCE->fdevents[i]->revents |= EV_WRITE;
            if (SOURCE->pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                SOURCE->fdevents[i]->revents |= EV_ERROR;
            }

            event_schedule(source->loop, &SOURCE->fdevents[i]->base);
            __list_del_init(&SOURCE->fdevents[i]->timeout_link);
        }
    }

    gettimeofday(&current, NULL);

    while (!__list_empty(&SOURCE->timeout_list)) {
        struct fdevent* event;

        event = __list_entry(SOURCE->timeout_list.next, struct fdevent, timeout_link);
        if (event->timeout.tv_sec > current.tv_sec || (current.tv_sec == event->timeout.tv_sec && event->timeout.tv_usec > current.tv_usec)) {
            break;
        }

        //remove_event(SOURCE, event);
        event->revents = EV_TIMEOUT;
        //DLOG("schedule event=%p", event);
        event_schedule(SOURCE->base.loop, &event->base);
    }

    return 0;
}

const struct event_source_ops fdevent_source_ops = {
    fdevent_new,
    fdevent_set,
    fdevent_get,
    fdevent_add,
    fdevent_del,
    fdevent_exec,
    fdevent_pending,
    fdevent_wait,
    fdevent_dispatch
};

struct event_source* fdevent_create(int maxfds)
{
    struct fdevent_source* source;

    source = malloc(sizeof(struct fdevent_source));
    if (!source)
        return NULL;

    memset(source, 0, sizeof(struct fdevent_source));
    source->base.ops = &fdevent_source_ops;
    source->pollfds = malloc(sizeof(struct pollfd) * maxfds);
    source->fdevents = malloc(sizeof(struct fdevent*) * maxfds);
    source->fdsize = maxfds;
    source->fdcount = 0;
    INIT_LIST_HEAD(&source->timeout_list);

    return (struct event_source*)source;
}
