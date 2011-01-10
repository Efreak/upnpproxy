#include "common.h"

#include "buf.h"

#include <stdio.h>
#include <string.h>

#define RUN_TEST(_test) \
    ++tot; cnt += _test ? 1 : 0

static bool test1(size_t size, size_t step);
static bool test2(size_t size, size_t step);
static bool test3(void);

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
