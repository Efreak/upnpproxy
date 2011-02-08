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

#include "buf.h"
#include <string.h>

struct _buf_t
{
    char* data, *rptr, *wptr, *end;
    bool full;
};

buf_t buf_new(size_t size)
{
    buf_t buf = malloc(sizeof(struct _buf_t) + size);
    if (buf == NULL)
        return NULL;
    buf->data = ((char*)buf) + sizeof(struct _buf_t);
    buf->rptr = buf->wptr = buf->data;
    buf->end = buf->data + size;
    buf->full = false;
    return buf;
}

void buf_free(buf_t buf)
{
    free(buf);
}

size_t buf_wavail(buf_t buf)
{
    if (buf->rptr > buf->wptr)
    {
        return buf->rptr - buf->wptr;
    }
    else if (buf->rptr == buf->wptr)
    {
        if (buf->full)
        {
            return 0;
        }
        return buf->end - buf->data;
    }
    else
    {
        return (buf->end - buf->wptr) + (buf->rptr - buf->data);
    }
}

size_t buf_ravail(buf_t buf)
{
    if (buf->wptr > buf->rptr)
    {
        return buf->wptr - buf->rptr;
    }
    else if (buf->wptr == buf->rptr)
    {
        if (buf->full)
        {
            return buf->end - buf->data;
        }
        return 0;
    }
    else
    {
        return (buf->end - buf->rptr) + (buf->wptr - buf->data);
    }
}

char* buf_wptr(buf_t buf, size_t* avail)
{
    if (buf->rptr > buf->wptr)
    {
        *avail = buf->rptr - buf->wptr;
    }
    else
    {
        if (buf->rptr == buf->wptr)
        {
            if (buf->full)
            {
                *avail = 0;
                return buf->wptr;
            }
            buf->rptr = buf->wptr = buf->data;
        }

        *avail = buf->end - buf->wptr;
    }
    return buf->wptr;
}

size_t buf_wmove(buf_t buf, size_t size)
{
    if (buf->rptr > buf->wptr)
    {
        assert(buf->rptr >= buf->wptr + size);
        buf->wptr += size;
        if (buf->rptr == buf->wptr)
        {
            buf->full = true;
            return 0;
        }
        return buf->rptr - buf->wptr;
    }
    else
    {
        assert(buf->rptr != buf->wptr || !buf->full || size == 0);
        assert(buf->wptr + size <= buf->end);
        buf->wptr += size;
        if (buf->wptr == buf->end)
        {
            buf->wptr = buf->data;
            if (buf->rptr == buf->wptr)
            {
                buf->full = true;
                return 0;
            }
            return buf->rptr - buf->wptr;
        }
        else
        {
            return buf->end - buf->wptr;
        }
    }
}

const char* buf_rptr(buf_t buf, size_t* avail)
{
    if (buf->wptr > buf->rptr)
    {
        *avail = buf->wptr - buf->rptr;
    }
    else
    {
        if (buf->rptr == buf->wptr)
        {
            if (!buf->full)
            {
                *avail = 0;
                return buf->rptr;
            }
        }

        *avail = buf->end - buf->rptr;
    }
    return buf->rptr;
}

size_t buf_rmove(buf_t buf, size_t size)
{
    if (buf->wptr > buf->rptr)
    {
        assert(buf->wptr >= buf->rptr + size);
        if (size > 0)
        {
            buf->full = false;
            buf->rptr += size;
        }
        return buf->wptr - buf->rptr;
    }
    else
    {
        assert(buf->rptr != buf->wptr || buf->full || size == 0);
        assert(buf->rptr + size <= buf->end);
        if (size > 0)
        {
            buf->full = false;
            buf->rptr += size;
        }
        if (buf->rptr == buf->end)
        {
            buf->rptr = buf->data;
            return buf->wptr - buf->rptr;
        }
        else
        {
            return buf->end - buf->rptr;
        }
    }
}

size_t buf_write(buf_t buf, const void* data, size_t size)
{
    const char* d = data;
    while (size > 0)
    {
        size_t avail;
        char* dst = buf_wptr(buf, &avail);
        if (avail == 0)
        {
            break;
        }
        if (size < avail)
        {
            avail = size;
        }
        memcpy(dst, d, avail);
        buf_wmove(buf, avail);
        size -= avail;
        d += avail;
    }
    return d - (const char*)data;
}

size_t buf_read(buf_t buf, void* data, size_t size)
{
    char* d = data;
    while (size > 0)
    {
        size_t avail;
        const char* src = buf_rptr(buf, &avail);
        if (avail == 0)
        {
            break;
        }
        if (size < avail)
        {
            avail = size;
        }
        memcpy(d, src, avail);
        buf_rmove(buf, avail);
        size -= avail;
        d += avail;
    }
    return d - (char*)data;
}

size_t buf_skip(buf_t buf, size_t size)
{
    size_t ret = 0;
    while (size > 0)
    {
        size_t avail;
        buf_rptr(buf, &avail);
        if (avail == 0)
        {
            break;
        }
        if (size < avail)
        {
            avail = size;
        }
        buf_rmove(buf, avail);
        size -= avail;
        ret += avail;
    }
    return ret;
}

size_t buf_peek(buf_t buf, void* data, size_t size)
{
    if (buf->wptr > buf->rptr)
    {
        size_t avail = buf->wptr - buf->rptr;
        if (avail > size) avail = size;
        memcpy(data, buf->rptr, avail);
        return avail;
    }
    else
    {
        size_t avail, fill;
        if (buf->rptr == buf->wptr)
        {
            if (!buf->full)
            {
                return 0;
            }
        }

        avail = buf->end - buf->rptr;
        if (avail > size)
        {
            avail = size;
        }
        memcpy(data, buf->rptr, avail);
        if (avail == size)
        {
            return avail;
        }
        fill = avail;
        size -= fill;
        avail = buf->wptr - buf->data;
        if (avail > size) avail = size;
        memcpy((char*)data + fill, buf->data, avail);
        return fill + avail;
    }
}

size_t buf_replace(buf_t buf, const void* data, size_t size)
{
    if (buf->wptr > buf->rptr)
    {
        size_t avail = buf->wptr - buf->rptr;
        if (avail > size) avail = size;
        memcpy(buf->rptr, data, avail);
        return avail;
    }
    else
    {
        size_t avail, done;
        if (buf->rptr == buf->wptr)
        {
            if (!buf->full)
            {
                return 0;
            }
        }

        avail = buf->end - buf->rptr;
        if (avail > size)
        {
            avail = size;
        }
        memcpy(buf->rptr, data, avail);
        if (avail == size)
        {
            return avail;
        }
        done = avail;
        size -= done;
        avail = buf->wptr - buf->data;
        if (avail > size) avail = size;
        memcpy(buf->data, (const char*)data + done, avail);
        return done + avail;
    }
}

buf_t buf_resize(buf_t buf, size_t newsize)
{
    size_t current = buf->end - buf->data;
    bool ok = false;
    size_t size;

    if (newsize < current)
    {
        if (newsize == 0)
        {
            return NULL;
        }
        if (buf_ravail(buf) > newsize)
        {
            /* Can't make the buffer smaller than the amount of data in it */
            return NULL;
        }
    }
    else if (newsize == current)
    {
        return buf;
    }

    if (buf->rptr < buf->wptr || (buf->rptr == buf->wptr && !buf->full))
    {
        char* tmp;
        size = buf->wptr - buf->rptr;
        memmove(buf->data, buf->rptr, size);
        tmp = realloc(buf, sizeof(struct _buf_t) + newsize);
        if (tmp != NULL)
        {
            ok = true;
            buf = (buf_t)tmp;
            buf->data = tmp + sizeof(struct _buf_t);
            buf->end = buf->data + newsize;
        }
    }
    else
    {
        /* Fallback case, do a full copy */
        char* tmp;
        size = buf_ravail(buf);
        tmp = malloc(sizeof(struct _buf_t) + newsize);
        if (tmp == NULL)
        {
            return NULL;
        }
        ok = true;
        buf_read(buf, tmp + sizeof(struct _buf_t), size);
        assert(buf_ravail(buf) == 0);
        free(buf);
        buf = (buf_t)tmp;
        buf->data = tmp + sizeof(struct _buf_t);
        buf->end = buf->data + newsize;
    }

    buf->rptr = buf->data;
    buf->wptr = buf->data + size;
    if ((buf->full = (buf->wptr == buf->end)))
    {
        buf->wptr = buf->data;
    }
    return ok ? buf : NULL;
}

size_t buf_size(buf_t buf)
{
    return buf->end - buf->data;
}

bool buf_rrotate(buf_t buf)
{
    if (buf->rptr == buf->data ||
        (buf->rptr == buf->wptr && !buf->full))
    {
        return false;
    }

    if (buf->rptr < buf->wptr)
    {
        size_t size = buf->wptr - buf->rptr;
        if (buf->rptr >= buf->data + size)
        {
            memcpy(buf->data, buf->rptr, size);
        }
        else
        {
            memmove(buf->data, buf->rptr, size);
        }
        buf->rptr = buf->data;
        buf->wptr = buf->data + size;
        return true;
    }
    if (buf->wptr < buf->rptr)
    {
        size_t a = buf->wptr - buf->data;
        size_t b = buf->rptr - buf->wptr;
        size_t c = buf->end - buf->rptr;
        if ((a + c) <= b)
        {
            if (a <= c)
            {
                memcpy(buf->data + c, buf->data, a);
            }
            else
            {
                memmove(buf->data + c, buf->data, a);
            }
            memcpy(buf->data, buf->rptr, c);
            buf->rptr = buf->data;
            buf->wptr = buf->data + (a + c);
            return true;
        }
    }

    /* fallback */
    {
        size_t size = buf_ravail(buf);
        char* tmp = malloc(size);
        buf_read(buf, tmp, size);
        assert(buf->rptr == buf->wptr && !buf->full);
        buf->rptr = buf->wptr = buf->data;
        buf_write(buf, tmp, size);
        free(tmp);
        assert(buf_ravail(buf) == size);
    }

    return true;
}
