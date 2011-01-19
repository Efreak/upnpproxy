/* Copyright (C) 2011, Joel Klinghed */

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
