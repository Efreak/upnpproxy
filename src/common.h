#ifndef COMMON_H
#define COMMON_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "compat.h"

#ifdef DEBUG
# include <assert.h>
#else
# define assert(__x) /* __x */
#endif

#endif /* COMMON_H */
