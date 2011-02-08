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

#include "util.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

char* trim(char* str)
{
    char* s, *e;
    for (s = str; is_space(*s); ++s);
    for (e = str + strlen(str); e > s && is_space(e[-1]); --e);
    *e = '\0';
    return s;
}

bool mkdir_p(const char* path)
{
    if (mkdir(path, 0777) == 0 || errno == EEXIST)
    {
        return true;
    }
    if (errno != ENOENT)
    {
        return false;
    }
    else
    {
        char* p = strdup(path);
        char* pos = strchr(p, '/');
        if (pos == p)
        {
            pos = strchr(p + 1, '/');
        }
        while (pos != NULL)
        {
            *pos = '\0';
            if (access(p, F_OK) != 0)
            {
                if (errno != ENOENT)
                {
                    free(p);
                    return false;
                }
                if (mkdir(p, 0777) != 0)
                {
                    free(p);
                    return false;
                }
            }
            *pos = '/';

            pos = strchr(pos + 1, '/');
        }
        free(p);
        return mkdir(path, 0777) == 0;
    }
}
