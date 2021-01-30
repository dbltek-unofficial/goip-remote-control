#ifndef __EVENT__H
#define __EVENT__H

#include <list.h>
#include <sys/time.h>

#define EV_TIMEOUT 0x01
#define EV_READ 0x02
#define EV_WRITE 0x04
#define EV_SIGNAL 0x08
#define EV_BUFREAD 0x10
#define EV_BUFWRITE 0x20
#define EV_ERROR 0x8000

struct event {
    struct event_source* source;
    int priority; /* smaller numbers are higher priority */
    struct list_head active_link;
    void (*callback)(void*, ...);
    void* arg;
};

#define EVP_CALLBACK 0x01
#define EVP_ARG 0x02
#define EVP_FD 0x03
#define EVP_TIMEOUT 0x04
#define EVP_EVENTS 0x05

int event_set(struct event*, int, const void*);
int event_get(struct event*, int, void*);
int event_add(struct event*);
void event_del(struct event*);

struct event_source {
    const struct event_source_ops* ops;
    struct list_head link;
    struct event_loop* loop;
};

struct event_source_ops {
    struct event* (*new_event)(struct event_source*, int events);
    int (*set)(struct event_source*, struct event*, int, const void*);
    int (*get)(struct event_source*, struct event*, int, void*);
    int (*add)(struct event_source*, struct event*);
    int (*del)(struct event_source*, struct event*);
    int (*exec)(struct event_source*, struct event*);
    int (*pending)(struct event_source*);
    int (*wait)(struct event_source*, int timeout);
    int (*dispatch)(struct event_source*);
};

struct event_loop {
    struct list_head pending;
    struct list_head sources;
    struct event_source* primary;
    int exit_status;
};

int event_loop_init(struct event_loop*);
int event_loop_add_source(struct event_loop*, struct event_source*, int primary);
int event_loop_del_source(struct event_loop*, struct event_source*);

struct event* event_loop_new_event(struct event_loop*, unsigned events);
void event_schedule(struct event_loop*, struct event*);
int event_loop(struct event_loop*);
void event_loop_exit(struct event_loop*, int code);

#endif
