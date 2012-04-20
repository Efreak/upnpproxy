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

#include "bitmap.h"
#include <string.h>

struct _bitmap_t
{
    uint32_t* data;
    size_t size, count;
    bool def;
};

bitmap_t bitmap_new(size_t size, bool def)
{
    bitmap_t bitmap = calloc(1, sizeof(struct _bitmap_t));
    bitmap->def = def;
    bitmap_resize(bitmap, size);
    return bitmap;
}

void bitmap_free(bitmap_t bitmap)
{
    if (bitmap != NULL)
    {
        free(bitmap->data);
        free(bitmap);
    }
}

size_t bitmap_size(bitmap_t bitmap)
{
    return bitmap->count;
}

void bitmap_resize(bitmap_t bitmap, size_t newsize)
{
    const size_t old = bitmap->count;
    if (bitmap->count == newsize)
    {
        return;
    }
    if (newsize > bitmap->size)
    {
        const size_t need = (newsize + 31) / 32;
        size_t ns = bitmap->size * 2;
        uint32_t* tmp;
        if (ns < need)
        {
            ns = need;
        }
        tmp = realloc(bitmap->data, ns * sizeof(uint32_t));
        if (tmp == NULL)
        {
            ns = need;
            tmp = realloc(bitmap->data, ns * sizeof(uint32_t));
            if (tmp == NULL)
            {
                abort();
            }
        }
        bitmap->size = ns;
        bitmap->data = tmp;
    }
    if (old < newsize)
    {
        bitmap_setrange(bitmap, old, newsize, bitmap->def);
    }
    bitmap->count = newsize;
}

bool bitmap_get(bitmap_t bitmap, size_t index)
{
    const uint32_t v = 1 << (index % 32);
    assert(index < bitmap->count);
    index /= 32;
    return (bitmap->data[index] & v) != 0;
}

void bitmap_set(bitmap_t bitmap, size_t index, bool value)
{
    const uint32_t v = 1 << (index % 32);
    assert(index < bitmap->count);
    index /= 32;
    if (value)
    {
        bitmap->data[index] |= v;
    }
    else
    {
        bitmap->data[index] &= ~v;
    }
}

void bitmap_setrange(bitmap_t bitmap, size_t start, size_t end, bool value)
{
    const size_t si1 = start / 32;
    size_t si2 = si1;
    const size_t ei = end / 32;
    const size_t sm = start % 32;
    const size_t em = end % 32;
    size_t i;
    if (sm != 0)
    {
        si2++;
    }
    assert(start <= end);
    assert(end <= bitmap->size);

    if (value)
    {
        for (i = 0; i < sm; i++)
        {
            bitmap->data[si1] |= 1 << (31 - i);
        }

        memset(bitmap->data + si2, 0xff, (ei - si2) * sizeof(uint32_t));

        for (i = 0; i < em; i++)
        {
            bitmap->data[ei] |= 1 << i;
        }
    }
    else
    {
        for (i = 0; i < sm; i++)
        {
            bitmap->data[si1] &= ~(1 << (31 - i));
        }

        memset(bitmap->data + si2, 0x00, (ei - si2) * sizeof(uint32_t));

        for (i = 0; i < em; i++)
        {
            bitmap->data[ei] &= ~(1 << i);
        }
    }
}
