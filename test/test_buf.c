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

#include <stdio.h>
#include <string.h>

#define RUN_TEST(_test) \
    ++tot; cnt += _test ? 1 : 0

static bool test1(size_t size, size_t step);
static bool test2(size_t size, size_t step);
static bool test3(void);
static bool test4(void);

int main(int argc, char** argv)
{
    unsigned int tot = 0, cnt = 0;

    RUN_TEST(test1(128, 1));
    RUN_TEST(test1(63, 1));
    RUN_TEST(test1(100, 5));
    RUN_TEST(test1(99, 6));

    RUN_TEST(test2(128, 1));
    RUN_TEST(test2(63, 1));
    RUN_TEST(test2(100, 5));
    RUN_TEST(test2(99, 6));

    RUN_TEST(test3());

    RUN_TEST(test4());

    fprintf(stdout, "OK %u/%u\n", cnt, tot);

    return cnt == tot ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool test1(size_t size, size_t step)
{
    buf_t buf = buf_new(size);
    char* tmp1, *tmp2;
    size_t i, j, done = 1;

    if (buf_wavail(buf) != size)
    {
        fprintf(stderr, "test1:%lu:%lu: buf_wavail fails sanity check\n",
                size, step);
        buf_free(buf);
        return false;
    }
    if (buf_ravail(buf) != 0)
    {
        fprintf(stderr, "test1:%lu:%lu buf_ravail fails sanity check\n",
                size, step);
        buf_free(buf);
        return false;
    }

    tmp1 = malloc(size);
    tmp2 = malloc(size);

    for (i = 1; i < size; i += step)
    {
        memset(tmp1, i, i);
        j = buf_write(buf, tmp1, i);
        while (j < i)
        {
            size_t x = buf_read(buf, tmp2, done), y;
            if (x != done)
            {
                fprintf(stderr,
                        "test1:%lu:%lu buf_read(%lu) returned %lu\n",
                        size, step, done, x);
                free(tmp1);
                free(tmp2);
                buf_free(buf);
                return false;
            }
            for (y = 0; y < x; ++y)
            {
                if (tmp2[y] != done)
                {
                    fprintf(stderr,
                            "test1:%lu:%lu buf_read(%lu)[%lu] != %u\n",
                            size, step, done, y, (unsigned int)done);
                    free(tmp1);
                    free(tmp2);
                    buf_free(buf);
                    return false;
                }
            }
            done += step;
            j += buf_write(buf, tmp1 + j, i - j);
        }
    }
    while (done < size)
    {
        size_t x = buf_read(buf, tmp2, done), y;
        if (x != done)
        {
            fprintf(stderr,
                    "test1:%lu:%lu buf_read(%lu) returned %lu\n",
                    size, step, done, x);
            free(tmp1);
            free(tmp2);
            buf_free(buf);
            return false;
        }
        for (y = 0; y < x; ++y)
        {
            if (tmp2[y] != done)
            {
                fprintf(stderr,
                        "test1:%lu:%lu buf_read(%lu)[%lu] != %u\n",
                        size, step, done, y, (unsigned int)done);
                free(tmp1);
                free(tmp2);
                buf_free(buf);
                return false;
            }
        }
        done += step;
    }

    free(tmp1);
    free(tmp2);
    buf_free(buf);
    return true;
}

static bool test2(size_t size, size_t step)
{
    buf_t buf = buf_new(size);
    char* tmp1, *tmp2;
    size_t i, j, done = 2;

    if (buf_wavail(buf) != size)
    {
        fprintf(stderr, "test2:%lu:%lu: buf_wavail fails sanity check\n",
                size, step);
        buf_free(buf);
        return false;
    }
    if (buf_ravail(buf) != 0)
    {
        fprintf(stderr, "test2:%lu:%lu buf_ravail fails sanity check\n",
                size, step);
        buf_free(buf);
        return false;
    }

    tmp1 = malloc(size);
    tmp2 = malloc(size);
    step *= 2;

    for (i = 2; i < size; i += step)
    {
        memset(tmp1, i, i);
        j = buf_write(buf, tmp1, i);
        while (j < i)
        {
            size_t x, y;
            size_t a = done / 2;
            buf_skip(buf, a);
            x = buf_read(buf, tmp2, a);
            if (x != a)
            {
                fprintf(stderr,
                        "test2:%lu:%lu buf_read(%lu) returned %lu\n",
                        size, step, done, x);
                free(tmp1);
                free(tmp2);
                buf_free(buf);
                return false;
            }
            for (y = 0; y < x; ++y)
            {
                if (tmp2[y] != done)
                {
                    fprintf(stderr,
                            "test2:%lu:%lu buf_read(%lu)[%lu] != %u\n",
                            size, step, done, y, (unsigned int)done);
                    free(tmp1);
                    free(tmp2);
                    buf_free(buf);
                    return false;
                }
            }
            done += step;
            j += buf_write(buf, tmp1 + j, i - j);
        }
    }
    while (done < size)
    {
        size_t x = buf_read(buf, tmp2, done), y;
        if (x != done)
        {
            fprintf(stderr,
                    "test2:%lu:%lu buf_read(%lu) returned %lu\n",
                    size, step, done, x);
            free(tmp1);
            free(tmp2);
            buf_free(buf);
            return false;
        }
        for (y = 0; y < x; ++y)
        {
            if (tmp2[y] != done)
            {
                fprintf(stderr,
                        "test2:%lu:%lu buf_read(%lu)[%lu] != %u\n",
                        size, step, done, y, (unsigned int)done);
                free(tmp1);
                free(tmp2);
                buf_free(buf);
                return false;
            }
        }
        done += step;
    }

    free(tmp1);
    free(tmp2);
    buf_free(buf);
    return true;
}

static bool test3(void)
{
    buf_t buf = buf_new(150);
    unsigned char* tmp = malloc(128);
    size_t x, y;

    for (x = 0; x < 128; ++x)
    {
        tmp[x] = x;
    }

    buf_write(buf, tmp, 128);

    x = buf_wavail(buf);
    if (x != (150 - 128))
    {
        fprintf(stderr, "test3: buf_wavail returned %lu\n",
                x);
        buf_free(buf);
        free(tmp);
        return false;
    }
    x = buf_ravail(buf);
    if (x != 128)
    {
        fprintf(stderr, "test3: buf_ravail returned %lu\n",
                x);
        buf_free(buf);
        free(tmp);
        return false;
    }

    memset(tmp, 0, 128);
    x = buf_peek(buf, tmp, 64);
    if (x != 64)
    {
        fprintf(stderr, "test3: buf_peek returned %lu\n",
                x);
        buf_free(buf);
        free(tmp);
        return false;
    }
    for (y = 0; y < x; ++y)
    {
        if (tmp[y] != y)
        {
            fprintf(stderr, "test3: buf_peek[%lu] != %lu\n",
                    y, y);
            buf_free(buf);
            free(tmp);
            return false;
        }
    }
    for (x = 0; x < 78; ++x)
    {
        tmp[x] = 'A' + x;
    }
    x = buf_skip(buf, 10);
    if (x != 10)
    {
        fprintf(stderr, "test3: buf_skip returned %lu\n",
                x);
        buf_free(buf);
        free(tmp);
        return false;
    }
    x = buf_replace(buf, tmp, 78);
    if (x != 78)
    {
        fprintf(stderr, "test3: buf_replace returned %lu\n",
                x);
        buf_free(buf);
        free(tmp);
        return false;
    }
    memset(tmp, 0, 128);
    x = buf_read(buf, tmp, 118);
    if (x != 118)
    {
        fprintf(stderr, "test3: buf_read returned %lu\n",
                x);
        buf_free(buf);
        free(tmp);
        return false;
    }
    for (x = 0; x < 78; ++x)
    {
        if (tmp[x] != 'A' + x)
        {
            fprintf(stderr, "test3: buf_read[%lu] != %lu but %u\n",
                    x, 'A' + x, tmp[x]);
            buf_free(buf);
            free(tmp);
            return false;
        }
    }
    for (; x < 118 - 78; ++x)
    {
        if (tmp[x] != 10 + x)
        {
            fprintf(stderr, "test3: buf_read[%lu] != %lu but %u\n",
                    x, 10 + x, tmp[x]);
            buf_free(buf);
            free(tmp);
            return false;
        }
    }
    buf_free(buf);
    free(tmp);
    return true;
}

static bool test4(void)
{
    buf_t buf = buf_new(20);
    char tmp1[20], tmp2[20];
    const char *ptr;
    size_t ret, i;
    for (i = 0; i < sizeof(tmp1); ++i)
    {
        tmp1[i] = 'A' + i;
    }
    if (buf_rrotate(buf))
    {
        fprintf(stderr, "test4: buf_rrotate returned false on empty buffer\n");
        buf_free(buf);
        return false;
    }
    ret = buf_write(buf, tmp1, 15);
    if (ret != 15)
    {
        fprintf(stderr, "test4: buf_write(15) failed: %lu\n", ret);
        buf_free(buf);
        return false;
    }
    if (buf_rrotate(buf))
    {
        fprintf(stderr, "test4: buf_rrotate returned false on already fixed buffer\n");
        buf_free(buf);
        return false;
    }
    ret = buf_read(buf, tmp2, 5);
    if (ret != 5)
    {
        fprintf(stderr, "test4: buf_read(5) failed: %lu\n", ret);
        buf_free(buf);
        return false;
    }
    ret = buf_ravail(buf);
    if (ret != 10)
    {
        fprintf(stderr, "test4: buf_ravail failed: %lu\n", ret);
        buf_free(buf);
        return false;
    }
    if (!buf_rrotate(buf))
    {
        fprintf(stderr, "test4: buf_rrotate returned true on buffer with 5 byte whole\n");
        buf_free(buf);
        return false;
    }
    ret = buf_ravail(buf);
    if (ret != 10)
    {
        fprintf(stderr, "test4: buf_ravail after buf_rrotate failed: %lu\n", ret);
        buf_free(buf);
        return false;
    }
    buf_rptr(buf, &ret);
    if (ret != 10)
    {
        fprintf(stderr, "test4: buf_rptr after buf_rrotate failed: %lu\n", ret);
        buf_free(buf);
        return false;
    }
    buf_wptr(buf, &ret);
    if (ret != 10)
    {
        fprintf(stderr, "test4: buf_wptr after buf_rrotate failed: %lu\n", ret);
        buf_free(buf);
        return false;
    }
    ret = buf_read(buf, tmp2, 10);
    if (ret != 10)
    {
        fprintf(stderr, "test4: buf_read after buf_rrotate failed: %lu\n", ret);
        buf_free(buf);
        return false;
    }
    if (memcmp(tmp2, tmp1 + 5, 10) != 0)
    {
        tmp2[10] = '\0';
        fprintf(stderr, "test4: got wrong data after buf_rrotate: `%s`\n", tmp2);
        buf_free(buf);
        return false;
    }
    assert(buf_ravail(buf) == 0);
    ret = buf_write(buf, tmp1, 20);
    assert(ret == 20);
    ret = buf_read(buf, tmp2, 12);
    assert(ret == 12);
    ret = buf_write(buf, tmp1, 7);
    assert(ret == 7);
    ret = buf_ravail(buf);
    if (ret != 15)
    {
        fprintf(stderr, "test4: second writes failed, ravail: %lu\n", ret);
        buf_free(buf);
        return false;
    }
    buf_rptr(buf, &ret);
    if (ret != 8)
    {
        fprintf(stderr, "test4: second writes failed, rptr(avail): %lu\n", ret);
        buf_free(buf);
        return false;
    }
    if (!buf_rrotate(buf))
    {
        fprintf(stderr, "test4: buf_rrotate after second write returned false\n");
        buf_free(buf);
        return false;
    }
    ret = buf_ravail(buf);
    if (ret != 15)
    {
        fprintf(stderr, "test4: second write, rrotate, failed, ravail: %lu\n", ret);
        buf_free(buf);
        return false;
    }
    ptr = buf_rptr(buf, &ret);
    if (ret != 15)
    {
        fprintf(stderr, "test4: second write, rptr, failed, ravail: %lu\n", ret);
        buf_free(buf);
        return false;
    }
    memcpy(tmp2, ptr, ret);
    tmp2[15] = '\0';
    if (memcmp(tmp2, tmp1 + 12, 8) != 0)
    {
        fprintf(stderr, "test4: second write, invalid data (first part): `%s`\n", tmp2);
        buf_free(buf);
        return false;
    }
    if (memcmp(tmp2 + 8, tmp1, 7) != 0)
    {
        fprintf(stderr, "test4: second write, invalid data (second part): `%s`\n", tmp2);
        buf_free(buf);
        return false;
    }
    buf_free(buf);
    return true;
}
