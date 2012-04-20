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

#include "map.h"
#include "bitmap.h"
#include <string.h>

struct _map_t
{
    size_t count, limit, tablesize, elementsize;
    map_hash_t hash_func;
    map_eq_t eq_func;
    map_free_t free_func;
    char* table;
    bitmap_t table_usage;
};

map_t map_new(size_t elementsize, map_hash_t hash_func, map_eq_t eq_func,
              map_free_t free_func)
{
    map_t map;
    assert(elementsize > 0 && hash_func != NULL && eq_func != NULL);
    map = calloc(1, sizeof(struct _map_t));
    map->hash_func = hash_func;
    map->eq_func = eq_func;
    map->free_func = free_func;
    map->elementsize = elementsize;
    return map;
}

void map_free(map_t map)
{
    if (map == NULL)
    {
        return;
    }

    if (map->count > 0 && map->free_func != NULL)
    {
        char* ptr = map->table;
        size_t i;
        for (i = 0; i < map->tablesize; i++, ptr += map->elementsize)
        {
            if (bitmap_get(map->table_usage, i))
            {
                map->free_func(ptr);
            }
        }
    }

    bitmap_free(map->table_usage);
    free(map->table);
    free(map);
}

size_t map_size(map_t map)
{
    return map->count;
}

void* map_put(map_t map, const void* element)
{
    size_t i;

    if (map->count == map->limit)
    {
        size_t ns = map->tablesize * 2;
        char* tmp, * ptr;
        bitmap_t usage;
        if (ns < 64)
        {
            ns = 64;
        }
        tmp = calloc(ns, map->elementsize);
        if (tmp == NULL)
        {
            ns = ((map->limit + 1) * 4 + 2) / 3;
            tmp = calloc(ns, map->elementsize);
            if (tmp == NULL)
            {
                assert(false);
                return NULL;
            }
        }
        usage = bitmap_new(ns, false);

        ptr = map->table;
        for (i = 0; i < map->tablesize; i++, ptr += map->elementsize)
        {
            if (bitmap_get(map->table_usage, i))
            {
                size_t ni = map->hash_func(ptr) % ns;
                for (;;)
                {
                    if (!bitmap_get(usage, ni))
                    {
                        break;
                    }
                    if (++ni == ns)
                    {
                        ni = 0;
                    }
                }
                bitmap_set(usage, ni, true);
                memcpy(tmp + ni * map->elementsize, ptr, map->elementsize);
            }
        }

        bitmap_free(map->table_usage);
        free(map->table);

        map->table = tmp;
        map->tablesize = ns;
        map->table_usage = usage;
        map->limit = (map->tablesize * 3) / 4;
        assert(map->limit > map->count);
    }

    i = map->hash_func(element) % map->tablesize;

    for (;;)
    {
        if (!bitmap_get(map->table_usage, i))
        {
            void* ptr = map->table + map->elementsize * i;
            bitmap_set(map->table_usage, i, true);
            memcpy(ptr, element, map->elementsize);
            map->count++;
            return ptr;
        }

        if (++i == map->tablesize)
        {
            i = 0;
        }
    }
}

void* map_get(map_t map, const void* element)
{
    char* ptr;
    size_t i;

    if (map->count == 0)
    {
        return NULL;
    }

    i = map->hash_func(element) % map->tablesize;
    ptr = map->table + map->elementsize * i;

    for (;;)
    {
        if (!bitmap_get(map->table_usage, i))
        {
            return NULL;
        }

        if (map->eq_func(ptr, element))
        {
            return ptr;
        }

        if (++i == map->tablesize)
        {
            ptr = map->table;
            i = 0;
        }
        else
        {
            ptr += map->elementsize;
        }
    }
}

size_t map_remove(map_t map, const void* element)
{
    char* ptr;
    size_t ret = 0, i;

    if (map->count == 0)
    {
        return ret;
    }

    i = map->hash_func(element) % map->tablesize;
    ptr = map->table + map->elementsize * i;

    for (;;)
    {
        if (!bitmap_get(map->table_usage, i))
        {
            return ret;
        }

        if (map->eq_func(ptr, element))
        {
            if (map->free_func != NULL)
            {
                map->free_func(ptr);
            }
            bitmap_set(map->table_usage, i, false);
            map->count--;
            ++ret;
        }

        if (++i == map->tablesize)
        {
            ptr = map->table;
            i = 0;
        }
        else
        {
            ptr += map->elementsize;
        }
    }

    return ret;
}

void* map_getat(map_t map, size_t idx)
{
    assert(idx < map->tablesize);
    if (bitmap_get(map->table_usage, idx))
    {
        return map->table + idx * map->elementsize;
    }
    else
    {
        return NULL;
    }
}

size_t map_begin(map_t map)
{
    size_t idx = 0;

    if (map->count == 0)
    {
        return map->tablesize;
    }

    for (;; idx++)
    {
        if (bitmap_get(map->table_usage, idx))
        {
            return idx;
        }
    }
}

size_t map_end(map_t map)
{
    return map->tablesize;
}

size_t map_next(map_t map, size_t idx)
{
    assert(idx <= map->tablesize);
    if (idx == map->tablesize)
    {
        return idx;
    }
    ++idx;
    for (; idx < map->tablesize; idx++)
    {
        if (bitmap_get(map->table_usage, idx))
        {
            break;
        }
    }
    return idx;
}

size_t map_removeat(map_t map, size_t idx)
{
    assert(idx <= map->tablesize);
    if (idx == map->tablesize)
    {
        return idx;
    }
    if (bitmap_get(map->table_usage, idx))
    {
        if (map->free_func != NULL)
        {
            map->free_func(map->table + idx * map->elementsize);
        }
        bitmap_set(map->table_usage, idx, false);
        map->count--;
    }
    return map_next(map, idx);
}
