#include "common.h"

#include "log.h"
#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

struct _log_t
{
    FILE* fh;
};

log_t log_open(void)
{
    log_t log = malloc(sizeof(struct _log_t));
    if (log == NULL)
        return NULL;
    log->fh = stderr;
    return log;
}

bool log_reopen(log_t log, const char* url)
{
    const char* pos = strchr(url, ':');
    FILE* fh;

    if (pos == NULL)
    {
        if (strcmp(url, "syslog") == 0)
        {
            if (log->fh == NULL)
            {
                closelog();
            }
            fh = NULL;
            openlog("upnpproxy", LOG_ODELAY | LOG_PID, LOG_DAEMON);
        }
        else
        {
            fh = fopen(url, "at");
            if (fh == NULL)
            {
                log_printf(log, LVL_ERR,
                           "Unable to open `%s` for appending: %s",
                           url, strerror(errno));
                return false;
            }
        }
    }
    else if (pos - url == 4 && memcmp(url, "file", 4) == 0)
    {
        fh = fopen(pos + 1, "at");
        if (fh == NULL)
        {
            log_printf(log, LVL_ERR, "Unable to open `%s` for appending: %s",
                       pos + 1, strerror(errno));
            return false;
        }
    }
    else if (pos - url == 6 && memcmp(url, "syslog", 6) == 0)
    {
        int facilty = LOG_DAEMON;
        ++pos;
        fh = NULL;
        if (log->fh == NULL)
        {
            closelog();
        }
        if (*pos == '\0')
        {
            facilty = LOG_DAEMON;
        }
        else
        {
            if (strcasecmp(pos, "user") == 0)
            {
                facilty = LOG_USER;
            }
            else if (strcasecmp(pos, "uucp") == 0)
            {
                facilty = LOG_UUCP;
            }
            else if (strcasecmp(pos, "news") == 0)
            {
                facilty = LOG_NEWS;
            }
            else if (strcasecmp(pos, "mail") == 0)
            {
                facilty = LOG_MAIL;
            }
            else if (strcasecmp(pos, "lpr") == 0)
            {
                facilty = LOG_LPR;
            }
            else if (strcasecmp(pos, "ftp") == 0)
            {
                facilty = LOG_FTP;
            }
            else if (strcasecmp(pos, "daemon") == 0)
            {
                facilty = LOG_DAEMON;
            }
            else if (strcasecmp(pos, "cron") == 0)
            {
                facilty = LOG_CRON;
            }
            else if (strcasecmp(pos, "auth") == 0)
            {
                facilty = LOG_AUTH;
            }
            else if (strcasecmp(pos, "authpriv") == 0)
            {
                facilty = LOG_AUTHPRIV;
            }
            else if (strcasecmp(pos, "local0") == 0)
            {
                facilty = LOG_LOCAL0;
            }
            else if (strcasecmp(pos, "local1") == 0)
            {
                facilty = LOG_LOCAL1;
            }
            else if (strcasecmp(pos, "local2") == 0)
            {
                facilty = LOG_LOCAL2;
            }
            else if (strcasecmp(pos, "local3") == 0)
            {
                facilty = LOG_LOCAL3;
            }
            else if (strcasecmp(pos, "local4") == 0)
            {
                facilty = LOG_LOCAL4;
            }
            else if (strcasecmp(pos, "local5") == 0)
            {
                facilty = LOG_LOCAL5;
            }
            else if (strcasecmp(pos, "local6") == 0)
            {
                facilty = LOG_LOCAL6;
            }
            else if (strcasecmp(pos, "local7") == 0)
            {
                facilty = LOG_LOCAL7;
            }
        }
        openlog("upnpproxy", LOG_ODELAY | LOG_PID, facilty);
    }
    else
    {
        log_printf(log, LVL_ERR, "Invalid log url: `%s`", url);
        return false;
    }

    if (log->fh != NULL)
    {
        if (log->fh != stderr)
        {
            fclose(log->fh);
        }
    }
    else
    {
        if (fh != NULL)
        {
            closelog();
        }
    }
    log->fh = fh;
    return true;
}

void log_close(log_t log)
{
    if (log->fh != NULL)
    {
        if (log->fh != stderr)
        {
            fclose(log->fh);
        }
    }
    else
    {
        closelog();
    }
    free(log);
}

static inline const char* lvl_str(log_lvl_t lvl)
{
    switch (lvl)
    {
    case LVL_ERR:
        return "Error: ";
    case LVL_WARN:
        return "Warning: ";
    case LVL_INFO:
        return "Info: ";
    }
    return "";
}

static inline int lvl_prio(log_lvl_t lvl)
{
    switch (lvl)
    {
    case LVL_ERR:
        return LOG_ERR;
    case LVL_WARN:
        return LOG_WARNING;
    case LVL_INFO:
        return LOG_INFO;
    }
    return LOG_ERR;
}

void log_puts(log_t log, log_lvl_t lvl, const char* msg)
{
    if (log->fh != NULL)
    {
        fputs(lvl_str(lvl), log->fh);
        fputs(msg, log->fh);
        fputc('\n', log->fh);
    }
    else
    {
        syslog(lvl_prio(lvl), "%s", msg);
    }
}

void log_printf(log_t log, log_lvl_t lvl, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    if (log->fh != NULL)
    {
        fputs(lvl_str(lvl), log->fh);
        vfprintf(log->fh, format, args);
        fputc('\n', log->fh);
    }
    else
    {
        vsyslog(lvl_prio(lvl), format, args);
    }
    va_end(args);
}
