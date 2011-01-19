/* Copyright (C) 2011, Joel Klinghed */

#ifndef TIMEVAL_H
#define TIMEVAL_H

#include <sys/time.h>

void timeval_add(struct timeval* target, const struct timeval* add);
void timeval_add2(struct timeval* target, unsigned long ms);
/* Returns x - y, return value is < 0 if result is negative, 0 if zero and > 0 if positive */
int timeval_diff(struct timeval* ret, const struct timeval* x, const struct timeval* y);
int timeval_cmp(const struct timeval* x, const struct timeval* y);

#endif /* TIMEVAL_H */
