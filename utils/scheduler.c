#include "scheduler.h"
#include "utils.h"

#include "btrace.h"
#include "debug.h"

void scheduler_init(struct scheduler* schd)
{
    bzero(schd, sizeof(*schd));
}

void scheduler_release(struct scheduler* schd)
{
    struct scheduler_entry* entry;

    if (schd->timer) {
        stop_timer(schd->timer);
    }

    entry = schd->entries;
    while (entry) {
        struct scheduler_entry* tmp;
        tmp = entry;
        entry = tmp->next;
        free(tmp);
    }
}

void scheduler_callback(struct scheduler* schd)
{
    struct scheduler_entry* pe;
    void (*task)(void*);
    void* ctxt;

    if (!schd->entries) {
        scheduler_disable(schd);
        return;
    }

    pe = schd->entries;
    schd->entries = pe->next;

    task = pe->task;
    ctxt = pe->ctxt;
    free(pe);
    CALL(task, (ctxt));

    if (!schd->entries) { //if no task added
        scheduler_disable(schd);
    }
}

int scheduler_enable(struct scheduler* schd)
{
    if (!schd->timer) {
        if (!(schd->timer = create_timer(0, schd, scheduler_callback))) {
            return -1;
        }
    }

    return start_timer(schd->timer, -1);
}

int scheduler_disable(struct scheduler* schd)
{
    if (schd->timer) {
        stop_timer(schd->timer);
    }
    return 0;
}

int scheduler_add_task(struct scheduler* schd, void* ctxt, void* task, int pior)
{
    struct scheduler_entry *entry, *pe;

    scheduler_delete_task(schd, ctxt, task);

    if (!(entry = malloc(sizeof(struct scheduler_entry)))) {
        return -1;
    }
    memset(entry, 0, sizeof(struct scheduler_entry));
    entry->ctxt = ctxt;
    entry->task = task;
    entry->pior = pior;
#if 0
	bzero(entry->call_stack, sizeof(entry->call_stack));
	btrace(entry->call_stack, 20);
#endif

    pe = schd->entries;
    if (!pe || pior < pe->pior) {
        entry->next = schd->entries;
        schd->entries = entry;
        if (!pe)
            scheduler_enable(schd);
    } else {
        while (pe->next) {
            if (pior > pe->next->pior) {
                entry->next = pe->next;
                pe->next = entry;
                return 0;
            }
            pe = pe->next;
        }
        pe->next = entry;
        entry->next = 0;
    }

    return 0;
}

int scheduler_delete_task(struct scheduler* schd, void* ctxt, void* task)
{
    struct scheduler_entry* pe;

    if (!(pe = schd->entries)) {
        return -1;
    }

    if (pe->ctxt == ctxt && pe->task == task) {
        schd->entries = pe->next;
        free(pe);
        return 0;
    } else {
        while (pe->next) {
            if (pe->next->ctxt == ctxt && pe->next->task == task) {
                struct scheduler_entry* tmp;

                tmp = pe->next;
                pe->next = tmp->next;
                free(tmp);
                return 0;
            }
            pe = pe->next;
        }
    }

    return -1;
}

void scheduler_delete_all(struct scheduler* schd, void* ctxt)
{
    struct scheduler_entry* pe;

    while (schd->entries && schd->entries->ctxt == ctxt) {
        pe = schd->entries;
        schd->entries = pe->next;
        free(pe);
    }

    if (!(pe = schd->entries)) {
        return;
    }
    while (pe->next) {
        if (pe->next->ctxt == ctxt) {
            struct scheduler_entry* tmp;

            tmp = pe->next;
            pe->next = tmp->next;
            free(tmp);
        } else {
            pe = pe->next;
        }
    }
}

int schedule_task(void* user_data, void* task, unsigned prior)
{
    return scheduler_add_task(&__app_thread_scheduler, user_data, task, prior);
}

void cancel_task(void* user_data, void* task)
{
    scheduler_delete_task(&__app_thread_scheduler, user_data, task);
}

void cancel_all_task(void* user_data, void* task)
{
    scheduler_delete_all(&__app_thread_scheduler, user_data);
}
