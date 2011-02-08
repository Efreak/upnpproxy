/*
 * Copyright (C) 2011, Joel Klinghed.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
    if (t != NULL)
    {
        if (timeval_cmp(&(timer->target), &(t->target)) <= 0)
        {
            timers->first = timer;
            t->prev = timer;
            timer->prev = NULL;
            timer->next = t;
            return;
        }
    }
    t = timers->last;
    if (t == NULL)
    {
        timers->first = timers->last = timer;
        timer->prev = NULL;
        timer->next = NULL;
        return;
    }
    if (timeval_cmp(&(timer->target), &(t->target)) > 0)
    {
        timers->last = timer;
        t->next = timer;
        timer->prev = t;
        timer->next = NULL;
        return;
    }
    for (;;)
    {
        if (timeval_cmp(&(timer->target), &(t->target)) <= 0)
        {
            timer->prev = t->prev;
            timer->next = t;
            t->prev->next = timer;
            t->prev = timer;
            return;
        }
        t = t->prev;
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
    timecb_remove(timer);
    free(timer);
}

void timecb_reschedule(timecb_t timer, unsigned long delay_ms)
{
    struct timeval target;
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

