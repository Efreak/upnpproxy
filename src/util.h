/* Copyright (C) 2011, Joel Klinghed */

#ifndef UTIL_H
#define UTIL_H

static inline bool is_space(char c)
{
    return (c == ' ' || c == '\t');
}

char* trim(char* str);

bool mkdir_p(const char* path);

#endif /* UTIL_H */
