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

#include "cfg.h"
#include "util.h"
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>

struct _cfg_t
{
    char* name;
    log_t log;
    char** value;
    size_t size, alloc;
};

static bool cfg_load(cfg_t cfg);

cfg_t cfg_open(const char* filename, log_t log)
{
    cfg_t cfg = calloc(1, sizeof(struct _cfg_t));
    if (cfg == NULL)
        return NULL;
    cfg->log = log;
    cfg->name = strdup(filename);
    if (!cfg_load(cfg))
    {
        cfg_close(cfg);
        return NULL;
    }
    return cfg;
}

void cfg_close(cfg_t cfg)
{
    size_t i;
    if (cfg == NULL)
        return;
    for (i = 0; i < cfg->size * 2; ++i)
    {
        free(cfg->value[i]);
    }
    free(cfg->value);
    free(cfg->name);
    free(cfg);
}

const char* cfg_getstr(cfg_t cfg, const char* key, const char* _default)
{
    size_t i, j;
    for (i = 0, j = 0; i < cfg->size; ++i, j += 2)
    {
        if (strcmp(cfg->value[j], key) == 0)
        {
            return cfg->value[j+1];
        }
    }
    return _default;
}

int cfg_getint(cfg_t cfg, const char* key, int _default)
{
    const char* str = cfg_getstr(cfg, key, NULL);
    long tmp;
    char* end = NULL;
    if (str == NULL)
        return _default;
    errno = 0;
    tmp = strtol(str, &end, 10);
    if (errno || end == NULL || *end != '\0' || tmp < INT_MIN || tmp > INT_MAX)
    {
        log_printf(cfg->log,
                   LVL_WARN, "%s: Value `%s` is not a valid integer: `%s`",
                   cfg->name, key, str);
        return _default;
    }
    return (int)tmp;
}

bool cfg_load(cfg_t cfg)
{
    char* line = NULL;
    size_t linelen = 0;
    FILE* fh;
    int ret;
    unsigned long num = 0;

    fh = fopen(cfg->name, "rt");
    if (fh == NULL)
    {
        log_printf(cfg->log, LVL_ERR, "Unable to open `%s` for reading: %s",
                   cfg->name, strerror(errno));
        return false;
    }

    while ((ret = getline(&line, &linelen, fh)) >= 0)
    {
        char* pos, *key, *value, *l;
        bool dupe = false;
        size_t i, j;
        ++num;
        while (ret > 0 && (line[ret - 1] == '\r' || line[ret - 1] == '\n'))
            --ret;
        if (line[ret] != '\0')
        {
            line[ret] = '\0';
        }
        l = trim(line);
        if (l[0] == '\0' || l[0] == '#')
        {
            /* Ignore empty lines or comments */
            continue;
        }
        pos = strchr(l, '=');
        if (pos == NULL)
        {
            log_printf(cfg->log, LVL_ERR, "%s:%lu: Invalid line: `%s`",
                       cfg->name, num, l);
            free(line);
            fclose(fh);
            return false;
        }
        *pos = '\0';
        ++pos;
        key = trim(l);
        value = trim(pos);
        for (i = 0, j = 0; i < cfg->size; ++i, j += 2)
        {
            if (strcmp(cfg->value[j], key) == 0)
            {
                log_printf(cfg->log, LVL_WARN,
                           "%s:%lu: Value `%s` is defined twice, ignoring the second defintion", cfg->name, num, key);
                dupe = true;
            }
        }
        if (!dupe)
        {
            if (cfg->size == cfg->alloc)
            {
                size_t na = cfg->alloc * 4;
                char** tmp;
                if (na < 8)
                    na = 8;
                tmp = realloc(cfg->value, na * sizeof(char*));
                if (tmp == NULL)
                {
                    na = cfg->size * 2 + 10;
                    tmp = realloc(cfg->value, na * sizeof(char*));
                    if (tmp == NULL)
                    {
                        log_printf(cfg->log, LVL_ERR,
                                   "%s:%lu: Invalid line: `%s`",
                                   cfg->name, num, l);
                        free(line);
                        fclose(fh);
                        return false;
                    }
                }
                cfg->alloc = na / 2;
                cfg->value = tmp;
            }
            cfg->value[cfg->size * 2 + 0] = strdup(key);
            cfg->value[cfg->size * 2 + 1] = strdup(value);
            ++(cfg->size);
        }
    }

    free(line);
    fclose(fh);
    return true;
}
