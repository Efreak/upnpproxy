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

#include "ssdp.h"
#include "http.h"
#include "util.h"
#include "vector.h"
#include <time.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

typedef struct _inet_t
{
    socket_t rsock, wsock;
    struct sockaddr* notify_host;
    socklen_t notify_hostlen;
} inet_t;

struct _ssdp_t
{
    log_t log;
    selector_t selector;
    timers_t timers;
    void* userdata;
    ssdp_search_callback_t search_cb;
    ssdp_search_response_callback_t search_response_cb;
    ssdp_notify_callback_t notify_cb;

    struct sockaddr* addrbuf;
    socklen_t addrbuflen, addrbufsize;

    inet_t inet4, inet6;

    vector_t search_responses;
};

typedef struct _search_response_t
{
    ssdp_t ssdp;
    http_resp_t resp;
    inet_t* inet;
    struct sockaddr* sender;
    socklen_t senderlen;
    timecb_t timer;
} search_response_t;

static void read_data(void* userdata, socket_t sock);
static void inet_setup(ssdp_t ssdp, const char* name, inet_t* inet,
                       bool bind, const char* bindaddr,
                       const char* any, const char* mcast,
                       uint16_t port);
static void inet_free(ssdp_t ssdp, inet_t* inet);

ssdp_t ssdp_new(log_t log, selector_t selector, timers_t timers,
                const char* bindaddr, void* userdata,
                ssdp_search_callback_t search_callback,
                ssdp_search_response_callback_t search_response_callback,
                ssdp_notify_callback_t notify_callback)
{
    ssdp_t ssdp = calloc(1, sizeof(struct _ssdp_t));

    if (ssdp == NULL)
    {
        return NULL;
    }
    ssdp->log = log;
    ssdp->selector = selector;
    ssdp->timers = timers;
    ssdp->userdata = userdata;
    ssdp->search_cb = search_callback;
    ssdp->search_response_cb = search_response_callback;
    ssdp->notify_cb = notify_callback;

    inet_setup(ssdp, "IPv4", &(ssdp->inet4),
               bindaddr == NULL || addrstr_is_ipv4(bindaddr), bindaddr,
               IPV4_ANY, "239.255.255.250", 1900);
    inet_setup(ssdp, "IPv6", &(ssdp->inet6),
               bindaddr == NULL || addrstr_is_ipv6(bindaddr), bindaddr,
               IPV6_ANY, "FF02::C", 1900);

    if (ssdp->inet4.rsock < 0 && ssdp->inet6.rsock < 0)
    {
        log_puts(log, LVL_ERR, "Unable to join any of IPv4 or IPv6 SSDP multicast group");
        inet_free(ssdp, &ssdp->inet4);
        inet_free(ssdp, &ssdp->inet6);
        free(ssdp);
        return NULL;
    }

    if (ssdp->inet4.wsock < 0 && ssdp->inet6.wsock < 0)
    {
        log_puts(log, LVL_ERR, "Unable to setup sending IPv4 or IPv6 SSDP multicast group");
        inet_free(ssdp, &ssdp->inet4);
        inet_free(ssdp, &ssdp->inet6);
        free(ssdp);
        return NULL;
    }

    ssdp->addrbuf = socket_allocate_addrbuffer(&(ssdp->addrbufsize));

    ssdp->search_responses = vector_new(sizeof(search_response_t*));

    return ssdp;
}

void inet_free(ssdp_t ssdp, inet_t* inet)
{
    if (inet->rsock >= 0)
    {
        selector_remove(ssdp->selector, inet->rsock);
        socket_close(inet->rsock);
        inet->rsock = -1;
    }
    if (inet->wsock >= 0)
    {
        selector_remove(ssdp->selector, inet->wsock);
        socket_close(inet->wsock);
        inet->wsock = -1;
    }
    free(inet->notify_host);
}

void inet_setup(ssdp_t ssdp, const char* name, inet_t* inet,
                bool bind,
                const char* bindaddr, const char* any, const char* mcast,
                uint16_t port)
{
    if (bind)
    {
        inet->rsock = socket_udp_listen(mcast, port);
        if (inet->rsock >= 0)
        {
            if (!socket_multicast_join(inet->rsock, mcast, bindaddr))
            {
                log_printf(ssdp->log, LVL_WARN,
                           "Error joining %s multicast group (%s): %s",
                           name, mcast, socket_strerror(inet->rsock));
                socket_close(inet->rsock);
                inet->rsock = -1;
            }
            else
            {
                selector_add(ssdp->selector, inet->rsock, ssdp,
                             read_data, NULL);
            }
        }
        else
        {
            log_printf(ssdp->log, LVL_WARN, "Error listening on %s:%u %s: %s",
                       mcast, port, name, socket_strerror(inet->rsock));
        }
    }
    else
    {
        inet->rsock = -1;
    }

    inet->wsock = socket_udp_listen(any, 0);
    if (inet->wsock >= 0)
    {
        socket_multicast_setttl(inet->wsock, 1);
        selector_add(ssdp->selector, inet->wsock, ssdp, read_data, NULL);
    }

    inet->notify_host = parse_addr(mcast, port, &(inet->notify_hostlen), false);
}

struct sockaddr* ssdp_getnotifyhost(ssdp_t ssdp, socklen_t* hostlen)
{
    struct sockaddr* addr;
    inet_t* inet;
    if (ssdp->inet4.rsock >= 0)
    {
        inet = &ssdp->inet4;
    }
    else if (ssdp->inet6.rsock >= 0)
    {
        inet = &ssdp->inet6;
    }
    else
    {
        if (hostlen != NULL) *hostlen = 0;
        return NULL;
    }
    if (hostlen != NULL) *hostlen = inet->notify_hostlen;
    addr = calloc(1, inet->notify_hostlen);
    memcpy(addr, inet->notify_host, inet->notify_hostlen);
    return addr;
}

static inet_t* select_inet(ssdp_t ssdp, const struct sockaddr* host, socklen_t hostlen)
{
    if (addr_is_ipv4(host, hostlen))
    {
        return &ssdp->inet4;
    }
    if (addr_is_ipv6(host, hostlen))
    {
        return &ssdp->inet6;
    }
    return NULL;
}

bool ssdp_search(ssdp_t ssdp, ssdp_search_t* search)
{
    char* tmp;
    http_req_t req;
    inet_t* inet;
    bool ret;
    assert(search && search->st && search->host);
    inet = select_inet(ssdp, search->host, search->hostlen);
    if (inet == NULL || inet->wsock < 0)
    {
        return false;
    }
    req = req_new("M-SEARCH", "*", "1.1");
    if (req == NULL)
    {
        return false;
    }
    asprinthost(&tmp, search->host, search->hostlen);
    req_addheader(req, "Host", tmp);
    free(tmp);
    if (search->s != NULL)
    {
        req_addheader(req, "S", search->s);
    }
    req_addheader(req, "ST", search->st);
    req_addheader(req, "Man", "\"ssdp:discover\"");
    if (asprintf(&tmp, "%u", search->mx) == -1)
    {
        req_free(req);
        return false;
    }
    req_addheader(req, "MX", tmp);
    free(tmp);
    ret = req_send(req, inet->wsock, inet->notify_host, inet->notify_hostlen,
                   ssdp->log);
    req_free(req);
    return ret;
}

static long search_response_cb(void* userdata)
{
    search_response_t* search_response = userdata;
    resp_send(search_response->resp, search_response->inet->rsock,
              search_response->sender, search_response->senderlen,
              search_response->ssdp->log);
    resp_free(search_response->resp);
    search_response->resp = NULL;
    search_response->timer = NULL;
    return -1;
}

bool ssdp_search_response(ssdp_t ssdp, ssdp_search_t* search,
                          ssdp_notify_t* notify)
{
    char* tmp;
    http_resp_t resp;
    bool ret;
    inet_t* inet;
    unsigned int delay;
    assert(search && notify && search->host && search->st && search->sender &&
           notify->expires >= time(NULL) && notify->usn);
    inet = select_inet(ssdp, search->sender, search->senderlen);
    if (inet == NULL || inet->rsock < 0)
    {
        return false;
    }
    resp = resp_new(200, "OK", "1.1");
    if (resp == NULL)
    {
        return false;
    }
    if (search->s != NULL)
    {
        resp_addheader(resp, "S", search->s);
    }
    resp_addheader(resp, "Ext", "");
    if (asprintf(&tmp, "no-cache=\"Ext\", max-age = %u",
                 ((unsigned int)(notify->expires - time(NULL)))) == -1)
    {
        resp_free(resp);
        return false;
    }
    resp_addheader(resp, "Cache-Control", tmp);
    free(tmp);
    resp_addheader(resp, "ST", search->st);
    resp_addheader(resp, "USN", notify->usn);
    resp_addheader(resp, "Location", notify->location);
    if (search->mx <= 0)
    {
        delay = 0;
    }
    else
    {
        /* Max delay, 2 hours and assume we took 0.5 seconds for processing */
        if (search->mx > 2 * 60 * 60) search->mx = 2 * 60 * 60;
        delay = rand() % ((search->mx * 1000) - 500);
    }
    if (delay <= 100)
    {
        ret = resp_send(resp, inet->rsock, search->sender, search->senderlen,
                        ssdp->log);
        resp_free(resp);
    }
    else
    {
        /* Delay response */
        size_t i;
        search_response_t* search_response = NULL;
        for (i = 0; i < vector_size(ssdp->search_responses); ++i)
        {
            search_response = *((search_response_t**)vector_get(ssdp->search_responses, i));
            if (search_response->timer == NULL)
            {
                break;
            }
            search_response = NULL;
        }
        if (search_response == NULL)
        {
            search_response = calloc(1, sizeof(search_response_t) + ssdp->addrbufsize);
            vector_push(ssdp->search_responses, &search_response);
            search_response->ssdp = ssdp;
            search_response->sender = (struct sockaddr*)((char*)search_response + sizeof(search_response_t));
        }

        search_response->resp = resp;
        search_response->inet = inet;
        search_response->senderlen = search->senderlen;
        memcpy(search_response->sender, search->sender, search->senderlen);
        search_response->timer = timers_add(ssdp->timers,
                                            delay,
                                            search_response,
                                            search_response_cb);
        ret = true;
    }
    return ret;
}

bool ssdp_notify(ssdp_t ssdp, ssdp_notify_t* notify)
{
    char* tmp;
    http_req_t req;
    bool ret;
    inet_t* inet;
    assert(notify && notify->host && notify->nt && notify->usn
           && notify->location && notify->expires >= time(NULL));
    inet = select_inet(ssdp, notify->host, notify->hostlen);
    if (inet == NULL || inet->wsock < 0)
    {
        return false;
    }
    req = req_new("NOTIFY", "*", "1.1");
    if (req == NULL)
    {
        return false;
    }
    asprinthost(&tmp, notify->host, notify->hostlen);
    req_addheader(req, "Host", tmp);
    free(tmp);
    req_addheader(req, "NT", notify->nt);
    req_addheader(req, "NTS", "ssdp:alive");
    req_addheader(req, "USN", notify->usn);
    req_addheader(req, "Location", notify->location);
    if (asprintf(&tmp, "max-age = %u",
                 ((unsigned int)(notify->expires - time(NULL)))) == -1)
    {
        req_free(req);
        return false;
    }
    req_addheader(req, "Cache-Control", tmp);
    if (notify->server != NULL)
        req_addheader(req, "Server", notify->server);
    if (notify->opt != NULL)
        req_addheader(req, "OPT", notify->opt);
    if (notify->nls != NULL)
        req_addheader(req, "01-NLS", notify->nls);
    free(tmp);
    ret = req_send(req, inet->wsock, inet->notify_host, inet->notify_hostlen,
                   ssdp->log);
    req_free(req);
    return ret;
}

bool ssdp_byebye(ssdp_t ssdp, ssdp_notify_t* notify)
{
    char* tmp;
    http_req_t req;
    bool ret;
    inet_t* inet;
    assert(notify && notify->host && notify->nt && notify->usn);
    inet = select_inet(ssdp, notify->host, notify->hostlen);
    if (inet == NULL || inet->wsock < 0)
    {
        return false;
    }
    req = req_new("NOTIFY", "*", "1.1");
    if (req == NULL)
    {
        return false;
    }
    asprinthost(&tmp, notify->host, notify->hostlen);
    req_addheader(req, "Host", tmp);
    free(tmp);
    req_addheader(req, "NT", notify->nt);
    req_addheader(req, "NTS", "ssdp:byebye");
    req_addheader(req, "USN", notify->usn);
    ret = req_send(req, inet->wsock, inet->notify_host, inet->notify_hostlen,
                   ssdp->log);
    req_free(req);
    return ret;
}

void ssdp_free(ssdp_t ssdp)
{
    size_t i;
    if (ssdp == NULL)
        return;

    for (i = 0; i < vector_size(ssdp->search_responses); ++i)
    {
        search_response_t* search_response = *((search_response_t**)vector_get(ssdp->search_responses, i));
        if (search_response->timer != NULL)
        {
            timecb_cancel(search_response->timer);
            search_response->timer = NULL;
            resp_free(search_response->resp);
        }
        free(search_response);
    }
    vector_free(ssdp->search_responses);
    inet_free(ssdp, &ssdp->inet4);
    inet_free(ssdp, &ssdp->inet6);
    free(ssdp->addrbuf);
    free(ssdp);
}

static char* strtolower(char* str)
{
    char* s;
    for (s = str; *s != '\0'; ++s)
    {
        if (*s >= 'A' && *s <= 'Z')
        {
            *s = 'a' + (*s - 'A');
        }
    }
    return str;
}

static bool isws(const char* str, size_t len)
{
    const char* s = str;
    for (; len-- > 0; ++s)
    {
        if (!is_space(*s))
        {
            return false;
        }
    }
    return true;
}

static bool parse_host(char* str, struct sockaddr** addr,
                       socklen_t* addrlen)
{
    char* pos = strrchr(str, ':');
    uint16_t port;
    if (pos == NULL)
    {
        port = 1900;
    }
    else
    {
        unsigned long tmp;
        char* end;
        *pos = '\0';
        ++pos;
        errno = 0;
        tmp = strtoul(pos, &end, 10);
        if (errno || end == NULL || *end != '\0' || tmp >= 0xffff)
        {
            return false;
        }
        port = (uint16_t)tmp;
    }
    return ((*addr = parse_addr(str, port, addrlen, false)) != NULL);
}

void read_data(void* userdata, socket_t sock)
{
    ssdp_t ssdp = (ssdp_t)userdata;
    size_t fill;
    char buf[2048];
    ssize_t got;
    char* line, * next;
    ssdp_search_t search_data = {0,};
    ssdp_notify_t notify_data = {0,};
    bool search = false, err = false, notify = false;
    bool expect_search_response = false;

    if (sock == ssdp->inet4.wsock || sock == ssdp->inet6.wsock)
    {
        expect_search_response = true;
    }

    ssdp->addrbuflen = ssdp->addrbufsize;
    got = socket_udp_read(sock, buf, sizeof(buf),
                          ssdp->addrbuf, &(ssdp->addrbuflen));
    if (got < 0)
    {
        log_printf(ssdp->log, LVL_ERR, "Error reading from SSDP UDP multicast socket: %s", socket_strerror(sock));
        selector_remove(ssdp->selector, sock);
        if (sock == ssdp->inet4.wsock)
        {
            ssdp->inet4.wsock = -1;
        }
        else if (sock == ssdp->inet4.rsock)
        {
            ssdp->inet4.rsock = -1;
        }
        else if (sock == ssdp->inet6.wsock)
        {
            ssdp->inet6.wsock = -1;
        }
        else if (sock == ssdp->inet6.rsock)
        {
            ssdp->inet6.rsock = -1;
        }
        socket_close(sock);
        return;
    }
    if (got == 0)
    {
        return;
    }
    fill = got;

    if (fill < 4)
    {
        /* Not a HTTP(-like) request */
        return;
    }
    if (memcmp(buf + (fill - 4), "\r\n\r\n", 4) != 0)
    {
        /* Either not a HTTP(-like) request or it has a body */
        buf[fill - 1] = '\0';
        line = strstr(buf, "\r\n\r\n");
        if (line == NULL)
        {
            /* Not a HTTP(-like) request */
            return;
        }
        /* Cut off the body, we're not interested */
        fill = (line + 2) - buf;
        buf[fill] = '\0';
    }
    else
    {
        fill -= 2;
        buf[fill] = '\0';
    }
    line = buf;

    for (;;)
    {
        if (*line == '\0')
        {
            break;
        }
        next = strstr(line, "\r\n");
        if (next == NULL)
        {
            err = true;
            break;
        }
        *next = '\0';
        if (line == buf)
        {
            /* First line must be either M-SEARCH, NOTIFY req or
             * a M-SEARCH response */
            if (expect_search_response)
            {
                if (strcmp(line, "HTTP/1.1 200 OK") == 0)
                {
                    search = true;
                    notify = true;
                    search_data.sender = ssdp->addrbuf;
                    search_data.senderlen = ssdp->addrbuflen;
                }
            }
            else
            {
                if (strcmp(line, "M-SEARCH * HTTP/1.1") == 0)
                {
                    search = true;
                    search_data.sender = ssdp->addrbuf;
                    search_data.senderlen = ssdp->addrbuflen;
                }
                else if (strcmp(line, "NOTIFY * HTTP/1.1") == 0)
                {
                    notify = true;
                }
                else
                {
                    err = true;
                    break;
                }
            }
        }
        else
        {
            /* All other lines are attributes */
            char* pos = strchr(line, ':');
            char* key, * value;
            bool known = false;
            if (pos == NULL)
            {
                err = true;
                break;
            }
            *pos = '\0';
            key = trim(line);
            value = trim(pos + 1);

            if (search)
            {
                if (strcasecmp(key, "S") == 0)
                {
                    search_data.s = value;
                    known = true;
                }
                else if (strcasecmp(key, "Host") == 0)
                {
                    if (search_data.host != NULL ||
                        !parse_host(value, &(search_data.host),
                                    &(search_data.hostlen)))
                    {
                        err = true;
                        break;
                    }
                    known = true;
                }
                else if (strcasecmp(key, "Man") == 0)
                {
                    if (strcmp(value, "\"ssdp:discover\"") != 0)
                    {
                        err = true;
                        break;
                    }
                    known = true;
                }
                else if (strcasecmp(key, "MX") == 0)
                {
                    unsigned long tmp;
                    char* end;
                    errno = 0;
                    tmp = strtoul(value, &end, 10);
                    if (errno || end == NULL || *end != '\0')
                    {
                        err = true;
                        break;
                    }
                    search_data.mx = (uint)tmp;
                    known = true;
                }
                else if (strcasecmp(key, "ST") == 0)
                {
                    search_data.st = value;
                    known = true;
                }
            }
            if (notify && !known)
            {
                if (strcasecmp(key, "NT") == 0)
                {
                    notify_data.nt = value;
                }
                else if (strcasecmp(key, "Host") == 0)
                {
                    if (notify_data.host != NULL ||
                        !parse_host(value, &(notify_data.host),
                                    &(notify_data.hostlen)))
                    {
                        err = true;
                        break;
                    }
                }
                else if (strcasecmp(key, "NTS") == 0)
                {
                    if (strcmp(value, "ssdp:alive") == 0 ||
                        strcmp(value, "ssdp:byebye") == 0)
                    {
                        notify_data.nts = value;
                    }
                    else
                    {
                        err = true;
                        break;
                    }
                }
                else if (strcasecmp(key, "USN") == 0)
                {
                    notify_data.usn = value;
                }
                else if (strcasecmp(key, "Location") == 0)
                {
                    notify_data.location = value;
                }
                else if (strcasecmp(key, "AL") == 0)
                {
                    notify_data.location = value;
                }
                else if (strcasecmp(key, "Server") == 0)
                {
                    notify_data.server = value;
                }
                else if (strcasecmp(key, "OPT") == 0)
                {
                    notify_data.opt = value;
                }
                else if (strcasecmp(key, "01-NLS") == 0)
                {
                    notify_data.nls = value;
                }
                else if (strcasecmp(key, "Cache-Control") == 0)
                {
                    char* pos, * pos2;
                    unsigned long tmp;
                    char* end;
                    value = strtolower(value);
                    pos = strstr(value, "max-age");
                    if (pos == NULL)
                    {
                        err = true;
                        break;
                    }
                    pos += 7;
                    pos2 = strchr(pos, '=');
                    if (pos2 == NULL || (pos2 > pos && !isws(pos, pos2 - pos)))
                    {
                        err = true;
                        break;
                    }
                    value = trim(pos2 + 1);
                    errno = 0;
                    tmp = strtoul(value, &end, 10);
                    if (errno || end == NULL || (*end != '\0' && *end != ',' &&
                                                 *end != ';'))
                    {
                        err = true;
                        break;
                    }
                    notify_data.expires = time(NULL) + tmp;
                }
                else if (strcasecmp(key, "Expires") == 0)
                {
                    struct tm _tm;
                    char* pos = strptime(value, "%a, %d %b %Y %H:%M:%S %z",
                                         &_tm);
                    if (pos == NULL || *pos != '\0')
                    {
                        err = true;
                        break;
                    }
                    notify_data.expires = mktime(&_tm);
                }
            }
        }
        line = next + 2;
    }

    if (!err)
    {
        if (search)
        {
            if (notify)
            {
                if (ssdp->search_response_cb != NULL)
                {
                    ssdp->search_response_cb(ssdp->userdata, &search_data, &notify_data);
                }
            }
            else if (ssdp->search_cb != NULL)
            {
                ssdp->search_cb(ssdp->userdata, &search_data);
            }
        }
        else if (notify && ssdp->notify_cb)
        {
            ssdp->notify_cb(ssdp->userdata, &notify_data);
        }
    }

    free(search_data.host);
    free(notify_data.host);
}
