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

#include "vector.h"
#include <string.h>

struct _vector_t
{
    size_t elementsize, count, size;
    char* data;
};

vector_t vector_new(size_t elementsize)
{
    vector_t vector;
    assert(elementsize > 0);
    vector = calloc(1, sizeof(struct _vector_t));
    vector->elementsize = elementsize;
    return vector;
}

void vector_free(vector_t vector)
{
    if (vector == NULL)
    {
        return;
    }

    free(vector->data);
    free(vector);
}

size_t vector_size(vector_t vector)
{
    return vector->count;
}

void* vector_get(vector_t vector, size_t idx)
{
    assert(idx < vector->count);
    return vector->data + (idx * vector->elementsize);
}

void* vector_add(vector_t vector)
{
    void* ptr;
    if (vector->count == vector->size)
    {
        size_t ns = vector->size * 2;
        char* tmp;
        if (ns < vector->count)
            ns = vector->count;
        if (ns < 10)
            ns = 10;
        tmp = realloc(vector->data, ns * vector->elementsize);
        if (tmp == NULL)
        {
            ns = vector->count;
            tmp = realloc(vector->data, ns * vector->elementsize);
            if (tmp == NULL)
            {
                return NULL;
            }
        }
        vector->data = tmp;
        vector->size = ns;
    }
    ptr = vector->data + (vector->count * vector->elementsize);
    ++vector->count;
    memset(ptr, 0, vector->elementsize);
    return ptr;
}

void vector_set(vector_t vector, size_t idx, const void* data)
{
    if (idx >= vector->count)
    {
        vector->count = idx + 1;
        if (vector->count > vector->size)
        {
            size_t ns = vector->size * 2;
            char* tmp;
            if (ns < vector->count)
                ns = vector->count;
            if (ns < 10)
                ns = 10;
            tmp = realloc(vector->data, ns * vector->elementsize);
            if (tmp == NULL)
            {
                ns = vector->count;
                tmp = realloc(vector->data, ns * vector->elementsize);
                if (tmp == NULL)
                {
                    return;
                }
            }
            vector->data = tmp;
            vector->size = ns;
        }
    }

    memcpy(vector->data + (idx * vector->elementsize), data,
           vector->elementsize);
}

void vector_insert(vector_t vector, size_t idx, const void* data)
{
    char* ptr;
    if (idx >= vector->count)
    {
        vector_set(vector, idx, data);
        return;
    }
    if (vector->count == vector->size)
    {
        size_t ns = vector->size * 2;
        char* tmp;
        if (ns < vector->count)
            ns = vector->count;
        if (ns < 10)
            ns = 10;
        tmp = realloc(vector->data, ns * vector->elementsize);
        if (tmp == NULL)
        {
            ns = vector->count;
            tmp = realloc(vector->data, ns * vector->elementsize);
            if (tmp == NULL)
            {
                return;
            }
        }
        vector->data = tmp;
        vector->size = ns;
    }
    ptr = vector->data + (idx * vector->elementsize);
    memmove(ptr + vector->elementsize, ptr,
            (vector->count - idx) * vector->elementsize);
    ++vector->count;
    memcpy(ptr, data, vector->elementsize);
}

void vector_push(vector_t vector, const void* data)
{
    vector_set(vector, vector->count, data);
}

void* vector_pop(vector_t vector, void* data)
{
    if (vector->count == 0)
    {
        return NULL;
    }
    memcpy(data, vector_get(vector, vector->count - 1), vector->elementsize);
    vector_remove(vector, vector->count - 1);
    return data;
}

void vector_remove(vector_t vector, size_t idx)
{
    char* ptr;
    assert(idx < vector->count);
    ptr = vector->data + idx * vector->elementsize;
    vector->count--;
    memmove(ptr, ptr + vector->elementsize,
            (vector->count - idx) * vector->elementsize);
}

void vector_removerange(vector_t vector, size_t begin, size_t end)
{
    const size_t len = end - begin;
    char* ptr;
    assert(begin <= end);
    assert(end <= vector->count);
    ptr = vector->data + begin * vector->elementsize;
    vector->count -= len;
    memmove(ptr, ptr + len * vector->elementsize,
            (vector->count - begin) * vector->elementsize);
}
