#include "common.h"

#include "timeval.h"

void timeval_add(struct timeval* target, const struct timeval* add)
{
    target->tv_sec += add->tv_sec;
    target->tv_usec += add->tv_usec;
    if (target->tv_usec >= 1000000)
    {
        target->tv_sec++;
        target->tv_usec -= 1000000;
    }
}

void timeval_add2(struct timeval* target, unsigned long ms)
{
    target->tv_sec += ms / 1000;
    target->tv_usec += (ms % 1000) * 1000;
    if (target->tv_usec >= 1000000)
    {
        target->tv_sec++;
        target->tv_usec -= 1000000;
    }
}

int timeval_diff(struct timeval* ret, const struct timeval* x, const struct timeval* y)
{
    struct timeval _y = *y;
    if (x->tv_usec < _y.tv_usec)
    {
        int nsec = (_y.tv_usec - x->tv_usec) / 1000000 + 1;
        _y.tv_usec -= 1000000 * nsec;
        _y.tv_sec += nsec;
    }
    if (x->tv_usec - _y.tv_usec > 1000000)
    {
        int nsec = (x->tv_usec - _y.tv_usec) / 1000000;
        _y.tv_usec += 1000000 * nsec;
        _y.tv_sec -= nsec;
    }

    ret->tv_sec = x->tv_sec - _y.tv_sec;
    ret->tv_usec = x->tv_usec - _y.tv_usec;

    return x->tv_sec < _y.tv_sec ? -1 : ((ret->tv_sec == 0 && ret->tv_usec == 0) ? 0 : 1);
}

int timeval_cmp(const struct timeval* x, const struct timeval* y)
{
    if (x->tv_sec < y->tv_sec)
    {
        return -1;
    }
    else if (x->tv_sec > y->tv_sec)
    {
        return 1;
    }

    if (x->tv_usec < y->tv_usec)
    {
        return -1;
    }
    else if (x->tv_usec > y->tv_usec)
    {
        return 1;
    }

    return 0;
}

