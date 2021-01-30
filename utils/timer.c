#include "timer.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "btrace.h"
#include "debug.h"

struct timer_entry {
    struct timer* timer;
    struct timeval timeout;
    int repeat;
    struct timer_entry* next;
};

static struct timer_entry* free_entries = 0;
static int in_check_timeout_lock = 0;
static struct timer_entry* timer_list = 0;

//static int pending_count=0;
static struct timer_entry* pending_list = 0;

int timer_avg_jitter = 0;

int tvcmp(struct timeval tv1, struct timeval tv2)
{
    return (tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000;
#if 0	
	if(tv1.tv_sec>tv2.tv_sec)return 1;
	else if(tv1.tv_sec<tv2.tv_sec)return -1;
	else {
		if(tv1.tv_usec>tv2.tv_usec)return 1;
		else if(tv1.tv_usec<tv2.tv_usec)return -1;
	}
	return 0;
#endif
}

void tvsub(struct timeval t1, struct timeval t2, struct timeval* res)
{
    res->tv_usec = t1.tv_usec - t2.tv_usec;
    if (res->tv_usec < 0) {
        res->tv_usec += 1000000;
        t1.tv_sec--;
    }
    res->tv_sec = t1.tv_sec - t2.tv_sec;
}

static void insert_timer(struct timer_entry* entry)
{
    struct timer_entry* ep;

    if (!timer_list || tvcmp(entry->timeout, timer_list->timeout) < 0) {
        entry->next = timer_list;
        timer_list = entry;
        return;
    }

    ep = timer_list;
    while (ep->next) {
        if (tvcmp(entry->timeout, ep->next->timeout) < 0) {
            break;
        }
        ep = ep->next;
    }

    entry->next = ep->next;
    ep->next = entry;
}

#if 0
#define DUMP_TIMERS dump_timers(__FUNCTION__)
static void dump_timers(const char *info) 
{
	struct timer_entry *entry;

	DPRINT("\n");
	DPRINT("%s(): %s\n", __FUNCTION__, info);
	entry=timer_list;
	while(entry){
		DPRINT("\t%d:%s()\n", entry->timer->interval, entry->timer->call_back_name);
		entry=entry->next;
	}
	entry=pending_list;
	
	while(entry){
		DPRINT("\t>%d:%s()\n", entry->timer->interval, entry->timer->call_back_name);
		entry=entry->next;
	}
	DPRINT("\n");
}
#else
#define DUMP_TIMERS
#endif

static int remove_timer(struct timer* timer)
{
    struct timer_entry *entry, *pp;

    //	DLOG("remove timer: %p %s", timer->ctxt, addr_to_name(timer->call_back));
    entry = timer_list;
    pp = 0;
    while (entry) {
        if (entry->timer == timer) {
            if (!pp) {
                timer_list = entry->next;
            } else {
                pp->next = entry->next;
            }

            //free(entry);
            entry->next = free_entries;
            free_entries = entry;
            DUMP_TIMERS;
            return 0;
        }
        pp = entry;
        entry = pp->next;
    }

    entry = pending_list;
    pp = 0;
    while (entry) {
        if (entry->timer == timer) {
            if (!pp) {
                pending_list = entry->next;
            } else {
                pp->next = entry->next;
            }

            //free(entry);
            entry->next = free_entries;
            free_entries = entry;
            DUMP_TIMERS;
            //pending_count--;
            //DPRINT(__FUNCTION__"(): pending_count=%d\n", pending_count);
            return 0;
        }
        pp = entry;
        entry = pp->next;
    }

    return -1;
}

static void pend_timer(struct timer_entry* entry)
{
    entry->next = pending_list;
    pending_list = entry;
}

static void add_timer(struct timer_entry* entry)
{
    struct timeval tv;
    unsigned sec, usec;

    gettimeofday(&tv, 0);
    if (tv.tv_usec > 1000000 || tv.tv_usec < 0) {
        tv.tv_usec = 0;
    }

    sec = entry->timer->interval / 1000;
    usec = (entry->timer->interval % 1000) * 1000;

    tv.tv_usec += usec;
    if (tv.tv_usec > 1000000) {
        tv.tv_usec -= 1000000;
        sec++;
    }
    tv.tv_sec += sec;

    entry->timeout = tv;

    if (in_check_timeout_lock) {
        //DLOG("pend timer: %p %s", entry->timer->ctxt, addr_to_name(entry->timer->call_back));
        pend_timer(entry);
        return;
    }
    insert_timer(entry);
    //DLOG("add timer: %p %s", entry->timer->ctxt, addr_to_name(entry->timer->call_back));

    DUMP_TIMERS;
}

int timer_check_timeout()
{
    struct timer_entry* e;
    struct timeval tv;
    int timeout;
    int res;

    gettimeofday(&tv, 0);
    if (tv.tv_usec > 1000000 || tv.tv_usec < 0) {
        tv.tv_usec = 0;
    }

    in_check_timeout_lock = 1;
    //DLOG("locked and checking...");
    while (timer_list && (res = tvcmp(tv, timer_list->timeout)) >= 0) {
        struct timer* timer;

        if (res > 0) {
            if (timer_avg_jitter == 0) {
                timer_avg_jitter = res;
            } else {
                timer_avg_jitter += res;
                timer_avg_jitter >>= 1;
            }
            //DLOG("jitter: %d", res);
        }
        e = timer_list;
        timer_list = e->next;
        timer = e->timer;

        if (--e->repeat) {
            add_timer(e);
        } else {
            e->next = free_entries;
            free_entries = e;
            //free(e);
        }
        //DLOG("timer timeout: timer=%p ctxt=%p next=%p callback=%s", timer, timer->ctxt, timer_list?timer_list->timer:0, addr_to_name(timer->call_back));
        CALL(timer->call_back, (timer->ctxt));
    }
    //DLOG("done and unlock");
    in_check_timeout_lock = 0;

    e = pending_list;
    while (e) {
        struct timer_entry* current = e;
        e = e->next;
        //	DLOG("add pending timer: %p %s", current->timer->ctxt, addr_to_name(current->timer->call_back));
        insert_timer(current);
    }
    pending_list = 0;

    if (!timer_list) {
        //	DLOG("timer list empty!");
        return -1;
    }

    gettimeofday(&tv, 0);
    if (tv.tv_usec > 1000000 || tv.tv_usec < 0) {
        tv.tv_usec = 0;
    }

    tvsub(timer_list->timeout, tv, &tv);
    timeout = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    //DLOG("next timeout: %d %s\n", timeout, addr_to_name(timer_list->timer->call_back));
#if 0
	if(timeout<0 || timeout >30 ){
		DLOG("abnormal: %d", timeout);
	}
#endif
    if (timeout < 0)
        timeout = 0;

    return timeout;
}

struct timer* _create_timer(unsigned interval, void* ctxt, void* call_back, const char* call_back_name)
{
    struct timer* timer;

    if (!(timer = (struct timer*)malloc(sizeof(struct timer)))) {
        return 0;
    }

    timer->interval = interval;
    timer->ctxt = ctxt;
    timer->call_back = call_back;
    timer->call_back_name = call_back_name;

    return timer;
}

void destroy_timer(struct timer* timer)
{
    remove_timer(timer);
    free(timer);
}

int start_timer(struct timer* timer, int repeat)
{
    struct timer_entry* entry;

    remove_timer(timer);

    if (free_entries) {
        entry = free_entries;
        free_entries = entry->next;
    } else {
        if (!(entry = (struct timer_entry*)malloc(sizeof(struct timer_entry)))) {
            return -1;
        }
    }
    entry->timer = timer;
    entry->repeat = repeat;

    add_timer(entry);

    return 0;
}

int set_timeout(struct timer* timer, struct timeval* timeout)
{
    struct timer_entry* entry;

    remove_timer(timer);

    if (free_entries) {
        entry = free_entries;
        free_entries = entry->next;
    } else {
        if (!(entry = (struct timer_entry*)malloc(sizeof(struct timer_entry)))) {
            return -1;
        }
    }
    entry->timer = timer;
    entry->repeat = 1;
    entry->timeout = *timeout;

    if (in_check_timeout_lock) {
        //DLOG("pend timer: %p %s", entry->timer->ctxt, addr_to_name(entry->timer->call_back));
        pend_timer(entry);
        return 0;
    }

    insert_timer(entry);
    return 0;
}

void stop_timer(struct timer* timer)
{
    remove_timer(timer);
}

static void* itimer_handler_ctxt = 0;
static void (*itimer_handler)(void*) = 0;

void sigalarm_handler(int sig)
{
    if (itimer_handler) {
        itimer_handler(itimer_handler_ctxt);
    }
    signal(SIGALRM, sigalarm_handler);
}

int register_itimer(void* ctxt, void* call_back)
{
    itimer_handler_ctxt = ctxt;
    itimer_handler = call_back;
    signal(SIGALRM, sigalarm_handler);

    return 0;
}

int start_itimer(int start, int interval)
{
    struct itimerval itv;

    itv.it_interval.tv_sec = interval / 1000;
    itv.it_interval.tv_usec = interval * 1000;

    itv.it_value.tv_sec = start / 1000;
    itv.it_value.tv_usec = start * 1000;

    signal(SIGALRM, sigalarm_handler);

    return setitimer(ITIMER_REAL, &itv, 0);
}

int stop_itimer()
{
    struct itimerval itv;

    bzero(&itv, sizeof(itv));
    return setitimer(ITIMER_REAL, &itv, 0);
}
