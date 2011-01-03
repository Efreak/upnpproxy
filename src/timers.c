#include "common.h"

#include "timers.h"
#include "timeval.h"

struct _timers_t
{
    timecb_t first, last;
};

struct _timecb_t
{
    timecb_t prev, next;
    struct timeval target;
    timers_t timers;
    void* userdata;
    timecb_callback_t callback;
    unsigned long delay;
};

#include <stdio.h>

timers_t timers_new(void)
{
    return calloc(1, sizeof(struct _timers_t));
}

void timers_free(timers_t timers)
{
    if (timers == NULL)
    {
        return;
    }

    while (timers->first != NULL)
    {
        timecb_cancel(timers->first);
    }
    free(timers);
}

static void timecb_insert(timecb_t timer)
{
    timers_t timers = timer->timers;
    timecb_t t = timers->first;
    fprintf(stderr, "insert: %p %lu.%06lu (first: %p last: %p)\n", timer, timer->target.tv_sec, timer->target.tv_usec, timers->first, timers->last);
    if (t != NULL)
    {
        fprintf(stderr, "first: %lu.%06lu\n", t->target.tv_sec, t->target.tv_usec);
        if (timeval_cmp(&(timer->target), &(t->target)) <= 0)
        {
            fprintf(stderr, "=> insert first\n");
            timers->first = timer;
            timer->prev = NULL;
            timer->next = t;
            return;
        }
    }
    t = timers->last;
    if (t == NULL)
    {
        fprintf(stderr, "=> insert first & last\n");
        timers->first = timers->last = timer;
        timer->prev = NULL;
        timer->next = NULL;
        return;
    }
    fprintf(stderr, "last: %lu.%06lu\n", t->target.tv_sec, t->target.tv_usec);
    if (timeval_cmp(&(timer->target), &(t->target)) > 0)
    {
        fprintf(stderr, "=> insert last\n");
        timers->last = timer;
        timer->prev = t;
        timer->next = NULL;
        return;
    }
    for (;;)
    {
        t = t->prev;
        fprintf(stderr, "t: %lu.%06lu\n", t->target.tv_sec, t->target.tv_usec);
        if (timeval_cmp(&(timer->target), &(t->target)) <= 0)
        {
            fprintf(stderr, "=> insert\n");
            timer->prev = t->prev;
            timer->next = t;
            t->prev = timer;
            return;
        }
    }
}

static void timecb_remove(timecb_t timer)
{
    if (timer->prev == NULL)
    {
        timer->timers->first = timer->next;
    }
    else
    {
        timer->prev->next = timer->next;
    }
    if (timer->next == NULL)
    {
        timer->timers->last = timer->prev;
    }
    else
    {
        timer->next->prev = timer->prev;
    }
    timer->prev = timer->next = NULL;
}

timecb_t timers_add(timers_t timers, unsigned long delay_ms,
                    void* userdata, timecb_callback_t callback)
{
    timecb_t timer;
    assert(callback != NULL);
    timer = calloc(1, sizeof(struct _timecb_t));
    timer->timers = timers;
    timer->delay = delay_ms;
    timer->userdata = userdata;
    timer->callback = callback;
    gettimeofday(&(timer->target), NULL);
    timeval_add2(&(timer->target), delay_ms);
    timecb_insert(timer);
    return timer;
}

unsigned long timers_tick(timers_t timers)
{
    for (;;)
    {
        struct timeval now, diff;
        if (timers->first == NULL)
        {
            return 0;
        }
        gettimeofday(&now, NULL);
        if (timeval_diff(&diff, &(timers->first->target), &now) <= 0)
        {
            timecb_t t = timers->first;
            long ret;
            timecb_remove(t);
            ret = t->callback(t->userdata);
            if (ret < 0)
            {
                fprintf(stderr, "t->callback: %ld t: %p t->userdata: %p\n",
                        ret, t, t->userdata);
                free(t);
                continue;
            }
            if (ret > 0)
            {
                t->delay = ret;
            }
            gettimeofday(&(t->target), NULL);
            timeval_add2(&(t->target), t->delay);
            timecb_insert(t);
            if (timers->first == t)
            {
                /* Make sure we don't loop */
                return 1;
            }
        }
        else
        {
            return (diff.tv_sec * 1000) + ((diff.tv_usec + 999) / 1000);
        }
    }
}

void timecb_cancel(timecb_t timer)
{
    assert(timer->prev != NULL || timer->next != NULL);
    timecb_remove(timer);
    free(timer);
}

void timecb_reschedule(timecb_t timer, unsigned long delay_ms)
{
    struct timeval target;
    assert(timer->prev != NULL || timer->next != NULL);
    gettimeofday(&target, NULL);
    timeval_add2(&target, delay_ms);
    timer->delay = delay_ms;
    if (timeval_cmp(&target, &(timer->target)) != 0)
    {
        timer->target = target;
        timecb_remove(timer);
        timecb_insert(timer);
    }
}

