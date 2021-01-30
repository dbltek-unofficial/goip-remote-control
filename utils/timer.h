#ifndef _TIMER_H
#define _TIMER_H

#include <sys/time.h>
#include <sys/types.h>

#define SECOND 1000

//typedef void (*TIMEOUTCALLBACK)(void *);

struct timer {
    unsigned interval;
    void* ctxt;
    void (*call_back)(void*);
    const char* call_back_name;
};

int tvcmp(struct timeval tv1, struct timeval tv2);
void tvsub(struct timeval t1, struct timeval t2, struct timeval* res);

#define create_timer(interval, ctxt, call_back) _create_timer(interval, ctxt, call_back, #call_back)
struct timer* _create_timer(unsigned interval, void* ctxt, void* call_back, const char* call_back_name);
void destroy_timer(struct timer* timer);

static inline void set_timer_interval(struct timer* timer, unsigned interval)
{
    timer->interval = interval;
}

static inline void set_timer_callback(struct timer* timer, void* callback)
{
    timer->call_back = callback;
}

int start_timer(struct timer* timer, int repeat);
void stop_timer(struct timer* timer);
int set_timeout(struct timer* timer, struct timeval* timeout);

void single_shot(unsigned delay, void* ctxt, void* call_back);

int register_itimer(void* ctxt, void* call_back);
int start_itimer(int start, int interval);
int stop_itimer();

#endif
