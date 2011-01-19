/* Copyright (C) 2011, Joel Klinghed */

#include "common.h"

#include "map.h"
#include <string.h>

struct _map_t
{
    size_t count, limit, tablesize, elementsize;
    map_hash_t hash_func;
    map_eq_t eq_func;
    map_free_t free_func;
    char* table;
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
    map->elementsize = elementsize + 1;
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
        char* ptr = map->table, * end = map->table + map->elementsize * map->tablesize;
        for (; ptr < end; ptr += map->elementsize)
        {
            if (*ptr != '\0')
            {
                map->free_func(ptr + 1);
            }
        }
    }

    free(map->table);
    free(map);
}

size_t map_size(map_t map)
{
    return map->count;
}

void* map_put(map_t map, const void* element)
{
    char* ptr, * end;

    if (map->count == map->limit)
    {
        size_t ns = map->tablesize * 2;
        char* tmp;
        if (ns < 64)
        {
            ns = 64;
        }
        tmp = realloc(map->table, ns * map->elementsize);
        if (tmp == NULL)
        {
            ns = ((map->limit + 1) * 4 + 2) / 3;
            tmp = realloc(map->table, ns * map->elementsize);
            if (tmp == NULL)
            {
                assert(false);
                return NULL;
            }
        }
        memset(tmp + map->tablesize * map->elementsize, 0,
               (ns - map->tablesize) * map->elementsize);
        map->table = tmp;
        map->tablesize = ns;
        map->limit = (map->tablesize * 3) / 4;
        assert(map->limit > map->count);
    }

    ptr = map->table + map->elementsize * (map->hash_func(element) % map->tablesize);
    end = map->table + map->elementsize * map->tablesize;

    for (;;)
    {
        if (*ptr == '\0')
        {
            *ptr = '\1';
            memcpy(ptr + 1, element, map->elementsize - 1);
            map->count++;
            return ptr + 1;
        }

        ptr += map->elementsize;
        if (ptr == end)
        {
            ptr = map->table;
        }
    }
}

void* map_get(map_t map, const void* element)
{
    char* ptr, * end;

    if (map->count == 0)
    {
        return NULL;
    }

    ptr = map->table + map->elementsize * (map->hash_func(element) % map->tablesize);
    end = map->table + map->elementsize * map->tablesize;

    for (;;)
    {
        if (*ptr == '\0')
        {
            return NULL;
        }

        if (map->eq_func(ptr + 1, element))
        {
            return ptr + 1;
        }

        ptr += map->elementsize;
        if (ptr == end)
        {
            ptr = map->table;
        }
    }
}

size_t map_remove(map_t map, const void* element)
{
    char* ptr, * end;
    size_t ret = 0;

    if (map->count == 0)
    {
        return ret;
    }

    ptr = map->table + map->elementsize * (map->hash_func(element) % map->tablesize);
    end = map->table + map->elementsize * map->tablesize;

    for (;;)
    {
        if (*ptr == '\0')
        {
            return ret;
        }

        if (map->eq_func(ptr + 1, element))
        {
            if (map->free_func != NULL)
            {
                map->free_func(ptr + 1);
            }
            *ptr = '\0';
            map->count--;
            ++ret;
        }

        ptr += map->elementsize;
        if (ptr == end)
        {
            ptr = map->table;
        }
    }

    return ret;
}

void* map_getat(map_t map, size_t idx)
{
    char* ptr;
    assert(idx < map->tablesize);
    ptr = map->table + idx * map->elementsize;
    if (*ptr != '\0')
    {
        return ptr + 1;
    }
    else
    {
        return NULL;
    }
}

size_t map_begin(map_t map)
{
    char* ptr;
    size_t idx = 0;

    if (map->count == 0)
    {
        return map->tablesize;
    }

    ptr = map->table;

    for (;; ptr += map->elementsize)
    {
        if (*ptr != '\0')
        {
            return idx;
        }
        else
        {
            ++idx;
        }
    }
}

size_t map_end(map_t map)
{
    return map->tablesize;
}

size_t map_next(map_t map, size_t idx)
{
    char* ptr, * end;
    assert(idx <= map->tablesize);
    if (idx == map->tablesize)
    {
        return idx;
    }
    ++idx;
    ptr = map->table + idx * map->elementsize;
    end = map->table + map->tablesize * map->elementsize;
    for (; ptr < end; ptr += map->elementsize)
    {
        if (*ptr != '\0')
        {
            return idx;
        }
        else
        {
            ++idx;
        }
    }
    return map->tablesize;
}

size_t map_removeat(map_t map, size_t idx)
{
    char* ptr, * end;
    assert(idx <= map->tablesize);
    if (idx == map->tablesize)
    {
        return idx;
    }
    ptr = map->table + idx * map->elementsize;
    if (*ptr != '\0')
    {
        if (map->free_func != NULL)
        {
            map->free_func(ptr + 1);
        }
        *ptr = '\0';
        map->count--;
    }
    ++idx;
    end = map->table + map->tablesize * map->elementsize;
    for (ptr += map->elementsize; ptr < end; ptr += map->elementsize)
    {
        if (*ptr != '\0')
        {
            return idx;
        }
        else
        {
            ++idx;
        }
    }
    return map->tablesize;
}
