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

#ifndef TIMERS_H
#define TIMERS_H

typedef struct _timers_t* timers_t;
typedef struct _timecb_t* timecb_t;

/* Return < 0 to be cancelled. Any pointers to timer is now obsolete.
 * Return == 0 to be repeated with the same delay.
 * Return > 0 to be repeated with a new delay (the returned) */
typedef long (* timecb_callback_t)(void* userdata);

timers_t timers_new(void);

void timers_free(timers_t timers);

timecb_t timers_add(timers_t timers, unsigned long delay_ms,
                    void* userdata, timecb_callback_t callback);

/* Return the maximum delay until next call to timers_tick in ms.
 * OBS! If there are no timers 0 is returned */
unsigned long timers_tick(timers_t timers);

/* These may not be called from inside the given timers callback, use the
 * return value for that */
void timecb_cancel(timecb_t timecb);
void timecb_reschedule(timecb_t timecb, unsigned long delay_ms);

#endif /* TIMERS_H */
