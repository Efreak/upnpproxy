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
#include <stdio.h>
#include <string.h>

#define RUN_TEST(_test) \
    ++tot; cnt += _test ? 1 : 0

static bool test_sanity(void);
static bool test_resize(void);

int main(int argc, char** argv)
{
    unsigned int tot = 0, cnt = 0;

    RUN_TEST(test_sanity());
    RUN_TEST(test_resize());

    fprintf(stdout, "OK %u/%u\n", cnt, tot);

    return cnt == tot ? EXIT_SUCCESS : EXIT_FAILURE;
}

static uint32_t strptr_hash(const void* element)
{
    const char* ptr = *((const char**)element);
    const uint32_t mask = 31;
    uint32_t value = 0;
    size_t i;
    for (i = 0; *ptr != '\0'; ++i, ++ptr)
    {
        value ^= (uint32_t)*ptr << ((i * 5) & mask);
    }
    return value;
}

static bool strptr_eq(const void* e1, const void* e2)
{
    const char* ptr1 = *((const char**)e1);
    const char* ptr2 = *((const char**)e2);
    return ptr1 == ptr2 || strcmp(ptr1, ptr2) == 0;
}

static void strptr_free(void* element)
{
    free(*((char**)element));
}

bool test_sanity(void)
{
    map_t map;
    char* ptr, ** pptr;
    const char* cptr;
    size_t ret, end;
    map = map_new(sizeof(char*), strptr_hash, strptr_eq, strptr_free);

    if (map_size(map) != 0)
    {
        fprintf(stderr, "test_sanity: initial map is not empty\n");
        map_free(map);
        return false;
    }

    ptr = strdup("test");
    pptr = map_put(map, &ptr);
    if (strcmp(*pptr, ptr) != 0)
    {
        fprintf(stderr, "test_sanity: map_put returned wrong data: %s\n",
                *pptr);
        map_free(map);
        return false;
    }

    if (map_size(map) != 1)
    {
        fprintf(stderr, "test_sanity: map_size should have been 1: %lu\n",
                map_size(map));
        map_free(map);
        return false;
    }

    ptr = strdup("hello");
    pptr = map_put(map, &ptr);
    if (strcmp(*pptr, ptr) != 0)
    {
        fprintf(stderr, "test_sanity: map_put returned wrong data: %s\n",
                *pptr);
        map_free(map);
        return false;
    }

    if (map_size(map) != 2)
    {
        fprintf(stderr, "test_sanity: map_size should have been 2: %lu\n",
                map_size(map));
        map_free(map);
        return false;
    }

    cptr = "hello";
    pptr = map_get(map, &cptr);
    if (pptr == NULL)
    {
        fprintf(stderr, "test_sanity: map_get should have returned result\n");
        map_free(map);
        return false;
    }
    else if (strcmp(*pptr, cptr) != 0)
    {
        fprintf(stderr, "test_sanity: map_get returned wrong result: %s\n",
                *pptr);
        map_free(map);
        return false;
    }

    cptr = "world";
    pptr = map_get(map, &cptr);
    if (pptr != NULL)
    {
        fprintf(stderr,
                "test_sanity: map_get should not have returned result: %s\n",
                *pptr);
        map_free(map);
        return false;
    }

    ret = map_begin(map);
    end = map_end(map);
    if (ret == end)
    {
        fprintf(stderr,
                "test_sanity: map_begin should have returned < map_end: %lu < %lu\n",
                ret, end);
        map_free(map);
        return false;
    }
    pptr = map_getat(map, ret);
    if (strcmp(*pptr, "test") == 0)
    {
        cptr = "hello";
    }
    else if (strcmp(*pptr, "hello") == 0)
    {
        cptr = "test";
    }
    else
    {
        fprintf(stderr,
                "test_sanity: map_getat(%lu) returned: %s\n", ret, *pptr);
        map_free(map);
        return false;
    }

    ret = map_next(map, ret);
    if (ret == end)
    {
        fprintf(stderr,
                "test_sanity: map_next(map_begin) should have returned < map_end: %lu < %lu\n",
                ret, end);
        map_free(map);
        return false;
    }
    pptr = map_getat(map, ret);
    if (strcmp(*pptr, cptr) != 0)
    {
        fprintf(stderr,
                "test_sanity: map_getat(%lu) returned: %s\n", ret, *pptr);
        map_free(map);
        return false;
    }
    ret = map_next(map, ret);
    if (ret != end)
    {
        fprintf(stderr,
                "test_sanity: map_next(map_next(map_begin)) should have returned map_end: %lu != %lu\n",
                ret, end);
        map_free(map);
        return false;
    }

    cptr = "world";
    ret = map_remove(map, &cptr);
    if (ret != 0)
    {
        fprintf(stderr,
                "test_sanity: map_remove should not have removed anything: %lu\n",
                ret);
        map_free(map);
        return false;
    }

    cptr = "hello";
    ret = map_remove(map, &cptr);
    if (ret != 1)
    {
        fprintf(stderr,
                "test_sanity: map_remove should have removed one item: %lu\n",
                ret);
        map_free(map);
        return false;
    }

    if (map_size(map) != 1)
    {
        fprintf(stderr,
                "test_sanity: map_size should have been 1 after remove: %lu\n",
                map_size(map));
        map_free(map);
        return false;
    }

    ret = map_removeat(map, map_begin(map));

    if (map_size(map) != 0)
    {
        fprintf(stderr,
                "test_sanity: map_size should have been 0 after removeat: %lu\n",
                map_size(map));
        map_free(map);
        return false;
    }

    if (ret != map_end(map))
    {
        fprintf(stderr,
                "test_sanity: map_removeat should have returned map_end\n");
        map_free(map);
        return false;
    }

    map_free(map);
    return true;
}

bool test_resize(void)
{
    const unsigned int count = 10000;
    map_t map;
    char* ptr, ** pptr;
    const char* cptr;
    char buf[20];
    unsigned int i;
    map = map_new(sizeof(char*), strptr_hash, strptr_eq, strptr_free);

    for (i = 0; i < count; i++)
    {
        snprintf(buf, sizeof(buf), "%u", i);
        ptr = strdup(buf);
        map_put(map, &ptr);
    }

    if (map_size(map) != count)
    {
        fprintf(stderr,
                "test_resize: map_size should have returned %u, not %lu\n",
                count, map_size(map));
        map_free(map);
        return false;
    }

    cptr = buf;
    for (i = 0; i < count; i++)
    {
        snprintf(buf, sizeof(buf), "%u", i);
        pptr = map_get(map, &cptr);
        if (pptr == NULL)
        {
            fprintf(stderr,
                    "test_resize: map_get('%s') should not have returned NULL\n",
                    buf);
            map_free(map);
            return false;
        }
    }

    cptr = buf;
    for (i = 0; i < count; i++)
    {
        snprintf(buf, sizeof(buf), "%u", i);
        map_remove(map, &cptr);
    }

    if (map_size(map) == 0)
    {
        fprintf(stderr, "test_resize: map_size != 0\n");
        map_free(map);
        return false;
    }

    map_free(map);
    return true;
}
