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

