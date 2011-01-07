#include "common.h"

#include "compat.h"
#include <string.h>

#if !HAVE_STRNLEN
size_t rpl_strnlen(const char* str, size_t max)
{
    size_t ret = 0;
    for (; *str != '\0'; ++str, ++ret);
    return ret;
}
#endif

#if !HAVE_STRNDUP
char* rpl_strndup(const char* str, size_t max)
{
    size_t len = strnlen(str, max);
    char* ret = malloc(len + 1);
    if (ret != NULL)
    {
        memcpy(ret, str, len);
        ret[len] = '\0';
    }
    return ret;
}
#endif

#if !HAVE_GETLINE
# include "rpl_getline.c"
#endif
