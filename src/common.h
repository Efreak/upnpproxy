/* Copyright (C) 2011, Joel Klinghed */

#ifndef COMMON_H
#define COMMON_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdbool.h>
#if HAVE_STDINT_H
# include <stdint.h>
#elif HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#include "compat.h"

#ifdef DEBUG
# include <assert.h>
#else
# define assert(__x) /* __x */
#endif

#endif /* COMMON_H */
