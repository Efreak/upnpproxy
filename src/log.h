/* Copyright (C) 2011, Joel Klinghed */

#ifndef LOG_H
#define LOG_H

typedef struct _log_t* log_t;
typedef enum _log_lvl_t
{
    LVL_ERR,
    LVL_WARN,
    LVL_INFO,
} log_lvl_t;

log_t log_open(void);

bool log_reopen(log_t log, const char* url);

void log_close(log_t log);

void log_puts(log_t log, log_lvl_t lvl, const char* msg);
void log_printf(log_t log, log_lvl_t lvl, const char* format, ...);

#endif /* LOG_H */
