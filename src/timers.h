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
