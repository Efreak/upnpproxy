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

#include "rpl_getline.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#define RUN_TEST(_test) \
    ++tot; cnt += _test ? 1 : 0

static bool test1(unsigned char cnt);
static char* root = NULL;

int main(int argc, char** argv)
{
    unsigned int tot = 0, cnt = 0;
    const char* srcdir = getenv("srcdir");

    if (srcdir != NULL)
    {
        root = strdup(srcdir);
    }
    else
    {
        root = strdup(".");
    }

    RUN_TEST(test1(1));
    RUN_TEST(test1(2));
    RUN_TEST(test1(3));

    fprintf(stdout, "OK %u/%u\n", cnt, tot);

    free(root);

    return cnt == tot ? EXIT_SUCCESS : EXIT_FAILURE;
}

static const char* test1data[] = { "1", "2", "", "3", "test" };

static bool test1(unsigned char cnt)
{
    char tmp[PATH_MAX];
    FILE* fh;
    unsigned int i = 0;
    bool ok = true;
    char* line = NULL;
    size_t linelen;
    int ret;
    snprintf(tmp, sizeof(tmp), "%s/data/test1-%u", root, cnt);
    fh = fopen(tmp, "rb");
    if (fh == NULL)
    {
        fprintf(stderr, "test1-%u: Unable to open `%s` for reading: %s\n",
                cnt, tmp, strerror(errno));
        return false;
    }
    while ((ret = rpl_getline(&line, &linelen, fh)) != -1)
    {
        ++i;
        if (i < 5)
        {
            if (ret == 0 || line[ret - 1] != '\n')
            {
                fprintf(stderr,
                        "test1-%u:%u: Line does not end with \\n: `%s`\n",
                        cnt, i, line);
                ok = false;
                continue;
            }
            line[ret - 1] = '\0';
        }
        else if (i == 5)
        {
            if (ret > 0 && line[ret - 1] == '\n')
            {
                fprintf(stderr,
                        "test1-%u:%u: Last line does end with \\n: `%s`\n",
                        cnt, i, line);
                ok = false;
                continue;
            }
        }
        else
        {
            fprintf(stderr,
                    "test1-%u:%u: Expected EOF, not: `%s`\n",
                    cnt, i, line);
            ok = false;
            continue;
        }

        if (strcmp(line, test1data[i - 1]) != 0)
        {
            fprintf(stderr,
                    "test1-%u:%u: Expected `%s` got `%s`\n",
                    cnt, i, test1data[i - 1], line);
            ok = false;
        }
    }
    free(line);
    if (!feof(fh))
    {
        if (ferror(fh))
        {
            fprintf(stderr, "test1-%u: Read error: %s\n",
                    cnt, strerror(errno));
            fclose(fh);
            return false;
        }
        fprintf(stderr, "test1-%u: getline returned -1 without EOF or error.",
                cnt);
ok = false;
    }
    fclose(fh);
    return ok;
}

#if HAVE_GETLINE
# include "rpl_getline.x"
#endif
