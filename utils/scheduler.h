
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "timer.h"

#define PIOR_URG 0
#define PIOR_HIGH 16
#define PIOR_MID 128
#define PIOR_LOW 256

struct scheduler {
    struct timer* timer;
    struct scheduler_entry {
        int pior;
        void* ctxt;
        void* task;
        struct scheduler_entry* next;
#ifndef NDEBUG
        void* call_stack[20];
#endif
    } * entries;
};

void scheduler_init(struct scheduler* schd);
void scheduler_release(struct scheduler* schd);
int scheduler_enable(struct scheduler* schd);
int scheduler_disable(struct scheduler* schd);
int scheduler_add_task(struct scheduler* schd, void* ctxt, void* task, int pior);
int scheduler_delete_task(struct scheduler* schd, void* ctxt, void* task);
void scheduler_delete_all(struct scheduler* schd, void* ctxt);

extern struct scheduler __app_thread_scheduler;

int schedule_task(void* user_data, void* task, unsigned prior);
void cancel_task(void* user_data, void* task);

#define SCHEDULE(ctxt, task, pior) scheduler_add_task(&__app_thread_scheduler, ctxt, task, pior)
#define CANCEL_ALL_SCHEDULE(ctxt) scheduler_delete_all(&__app_thread_scheduler, ctxt)

#endif
