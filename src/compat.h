/* Copyright (C) 2011, Joel Klinghed */

#ifndef COMPAT_H
#define COMPAT_H

#if !HAVE_STRNDUP
char* rpl_strndup(const char* str, size_t max);
#define strndup rpl_strndup
#endif

#if !HAVE_STRNLEN
size_t rpl_strnlen(const char* str, size_t max);
#define strnlen rpl_strnlen
#endif

#if !HAVE_GETLINE
#include "rpl_getline.h"
#define getline rpl_getline
#endif

#ifndef HOST_NAME_MAX
# define HOST_NAME_MAX (255)
#endif

#endif /* COMPAT_H */
