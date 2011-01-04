#include "common.h"

#include "ssdp.h"
#include "http.h"
#include "util.h"
#include <time.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

struct _ssdp_t
{
    log_t log;
    selector_t selector;
    void* userdata;
    ssdp_search_callback_t search_cb;
    ssdp_search_response_callback_t search_response_cb;
    ssdp_notify_callback_t notify_cb;

    struct sockaddr* notify_host;
    socklen_t notify_hostlen;

    socket_t sock_4, sock_6;
};

static void read_data(void* userdata, socket_t sock);

ssdp_t ssdp_new(log_t log,
                selector_t selector, const char* bindaddr, void* userdata,
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
    ssdp->userdata = userdata;
    ssdp->search_cb = search_callback;
    ssdp->search_response_cb = search_response_callback;
    ssdp->notify_cb = notify_callback;

    if (bindaddr != NULL)
    {
        ssdp->sock_4 = -1;
        ssdp->sock_6 = -1;
        if (addrstr_is_ipv4(bindaddr))
        {
            ssdp->sock_4 = socket_udp_listen(IPV4_ANY, 1900);
            if (ssdp->sock_4 >= 0)
            {
                if (!socket_setblocking(ssdp->sock_4, false) ||
                    !socket_multicast_join(ssdp->sock_4, "239.255.255.250",
                                           bindaddr))
                {
                    log_printf(log, LVL_WARN, "Error joining ipv4 multicast groudp (239.255.255.250): %s", socket_strerror(ssdp->sock_4));
                    socket_close(ssdp->sock_4);
                    ssdp->sock_4 = -1;
                }
            }
            else
            {
                log_printf(log, LVL_WARN, "Error listening on *:1900 ipv4: %s", socket_strerror(ssdp->sock_4));
            }
        }
        if (addrstr_is_ipv6(bindaddr))
        {
            ssdp->sock_6 = socket_udp_listen(IPV6_ANY, 1900);
            if (ssdp->sock_6 >= 0)
            {
                if (!socket_setblocking(ssdp->sock_6, false) ||
                    !socket_multicast_join(ssdp->sock_6, "FF02::C", bindaddr))
                {
                    log_printf(log, LVL_WARN, "Error joining ipv6 multicast groudp (FF02::C): %s", socket_strerror(ssdp->sock_6));
                    socket_close(ssdp->sock_6);
                    ssdp->sock_6 = -1;
                }
            }
            else
            {
                log_printf(log, LVL_WARN, "Error listening on *:1900 ipv6: %s", socket_strerror(ssdp->sock_6));
            }
        }
    }
    else
    {
        ssdp->sock_4 = socket_udp_listen(IPV4_ANY, 1900);
        if (ssdp->sock_4 >= 0)
        {
            if (!socket_setblocking(ssdp->sock_4, false) ||
                !socket_multicast_join(ssdp->sock_4, "239.255.255.250", NULL))
            {
                log_printf(log, LVL_WARN, "Error joining ipv4 multicast groudp (239.255.255.250): %s", socket_strerror(ssdp->sock_4));
                socket_close(ssdp->sock_4);
                ssdp->sock_4 = -1;
            }
        }

        ssdp->sock_6 = socket_udp_listen(IPV6_ANY, 1900);
        if (ssdp->sock_6 >= 0)
        {
            if (!socket_setblocking(ssdp->sock_6, false) ||
                !socket_multicast_join(ssdp->sock_6, "FF02::C", NULL))
            {
                log_printf(log, LVL_WARN, "Error joining ipv6 multicast groudp (FF02::C): %s", socket_strerror(ssdp->sock_6));
                socket_close(ssdp->sock_6);
                ssdp->sock_6 = -1;
            }
        }
    }

    if (ssdp->sock_4 == -1 && ssdp->sock_6 == -1)
    {
        free(ssdp);
        return NULL;
    }

    if (ssdp->sock_4 != -1)
    {
        selector_add(selector, ssdp->sock_4, ssdp, read_data, NULL);

        ssdp->notify_host = parse_addr("239.255.255.250", 1900,
                                       &ssdp->notify_hostlen, true);
    }
    if (ssdp->sock_6 != -1)
    {
        selector_add(selector, ssdp->sock_6, ssdp, read_data, NULL);

        ssdp->notify_host = parse_addr("FF02::C", 1900,
                                       &ssdp->notify_hostlen, true);
    }

    return ssdp;
}

struct sockaddr* ssdp_getnotifyhost(ssdp_t ssdp, socklen_t* hostlen)
{
    struct sockaddr* addr;
    if (hostlen != NULL)
        *hostlen = ssdp->notify_hostlen;
    addr = calloc(1, ssdp->notify_hostlen);
    memcpy(addr, ssdp->notify_host, ssdp->notify_hostlen);
    return addr;
}

bool ssdp_search_response(ssdp_t ssdp, ssdp_search_t* search,
                          ssdp_notify_t* notify)
{
    char* tmp;
    http_resp_t resp;
    bool ret;
    assert(search && notify && search->host && search->st &&
           notify->expires >= time(NULL) && notify->usn);
    if (addr_is_ipv4(search->host, search->hostlen) && ssdp->sock_4 < 0)
    {
        return false;
    }
    if (addr_is_ipv6(search->host, search->hostlen) && ssdp->sock_6 < 0)
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
    asprintf(&tmp, "no-cache=\"Ext\", max-age = %u",
             ((unsigned int)(notify->expires - time(NULL))));
    resp_addheader(resp, "Cache-Control", tmp);
    free(tmp);
    resp_addheader(resp, "ST", search->st);
    resp_addheader(resp, "USN", notify->usn);
    resp_addheader(resp, "Location", notify->location);
    if (addr_is_ipv4(search->host, search->hostlen))
    {
        ret = resp_send(resp, ssdp->sock_4, ssdp->log);
    }
    else
    {
        ret = resp_send(resp, ssdp->sock_6, ssdp->log);
    }
    resp_free(resp);
    return ret;
}

bool ssdp_notify(ssdp_t ssdp, ssdp_notify_t* notify)
{
    char* tmp;
    http_req_t req;
    bool ret;
    assert(notify && notify->host && notify->nt && notify->usn
           && notify->location && notify->expires >= time(NULL));
    if (addr_is_ipv4(notify->host, notify->hostlen) && ssdp->sock_4 < 0)
    {
        return false;
    }
    if (addr_is_ipv6(notify->host, notify->hostlen) && ssdp->sock_6 < 0)
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
    asprintf(&tmp, "max-age = %u",
             ((unsigned int)(notify->expires - time(NULL))));
    req_addheader(req, "Cache-Control", tmp);
    if (notify->server != NULL)
        req_addheader(req, "Server", notify->server);
    free(tmp);
    if (addr_is_ipv4(notify->host, notify->hostlen))
    {
        ret = req_send(req, ssdp->sock_4, ssdp->log);
    }
    else
    {
        ret = req_send(req, ssdp->sock_6, ssdp->log);
    }
    req_free(req);
    return ret;
}

bool ssdp_byebye(ssdp_t ssdp, ssdp_notify_t* notify)
{
    char* tmp;
    http_req_t req;
    bool ret;
    assert(notify && notify->host && notify->nt && notify->usn);
    if (addr_is_ipv4(notify->host, notify->hostlen) && ssdp->sock_4 < 0)
    {
        return false;
    }
    if (addr_is_ipv6(notify->host, notify->hostlen) && ssdp->sock_6 < 0)
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
    if (addr_is_ipv4(notify->host, notify->hostlen))
    {
        ret = req_send(req, ssdp->sock_4, ssdp->log);
    }
    else
    {
        ret = req_send(req, ssdp->sock_6, ssdp->log);
    }
    req_free(req);
    return ret;
}

void ssdp_free(ssdp_t ssdp)
{
    if (ssdp == NULL)
        return;

    if (ssdp->sock_4 != -1)
    {
        selector_remove(ssdp->selector, ssdp->sock_4);
        socket_close(ssdp->sock_4);
    }
    if (ssdp->sock_6 != -1)
    {
        selector_remove(ssdp->selector, ssdp->sock_6);
        socket_close(ssdp->sock_6);
    }
    free(ssdp->notify_host);
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
    char* ptr;
    size_t fill = 0, size;
    char buf[2048];
    ssize_t got;
    char* line, * next;
    ssdp_search_t search_data = {0,};
    ssdp_notify_t notify_data = {0,};
    bool search = false, err = false, notify = false;

    ptr = buf;
    size = sizeof(buf);

    for (;;)
    {
        size_t avail = size - fill;
        got = socket_read(sock, ptr + fill, avail);
        if (got < 0)
        {
            if (socket_blockingerror(sock))
            {
                return;
            }
            log_printf(ssdp->log, LVL_ERR, "Error reading from SSDP UDP multicast socket: %s", socket_strerror(sock));
            selector_remove(ssdp->selector, sock);
            if (sock == ssdp->sock_4)
            {
                ssdp->sock_4 = -1;
            }
            if (sock == ssdp->sock_6)
            {
                ssdp->sock_6 = -1;
            }
            socket_close(sock);
            return;
        }
        if (got == 0 && fill == 0)
        {
            return;
        }
        fill += got;
        if (got < avail)
        {
            break;
        }
        if (ptr == buf)
        {
            size_t ns = size * 2;
            char* tmp = malloc(ns);
            if (tmp == NULL)
            {
                return;
            }
            memcpy(tmp, ptr, fill);
            ptr = tmp;
            size = ns;
        }
        else
        {
            size_t ns = size * 2;
            char* tmp = realloc(ptr, ns);
            if (tmp == NULL)
            {
                free(ptr);
                return;
            }
            ptr = tmp;
            size = ns;
        }
    }

    if (fill < 4)
    {
        /* Not a HTTP(-like) request */
        if (ptr != buf) free(ptr);
        return;
    }
    if (memcmp(ptr + (fill - 4), "\r\n\r\n", 4) != 0)
    {
        /* Either not a HTTP(-like) request or it has a body */
        ptr[fill - 1] = '\0';
        line = strstr(ptr, "\r\n\r\n");
        if (line == NULL)
        {
            /* Not a HTTP(-like) request */
            if (ptr != buf) free(ptr);
            return;
        }
        /* Cut off the body, we're not interested */
        fill = (line + 2) - ptr;
        ptr[fill] = '\0';
    }
    else
    {
        fill -= 2;
        ptr[fill] = '\0';
    }
    line = ptr;
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
        if (line == ptr)
        {
            /* First line must be either M-SEARCH, NOTIFY req or
             * a M-SEARCH response */
            if (strcmp(line, "M-SEARCH * HTTP/1.1") == 0)
            {
                search = true;
            }
            else if (strcmp(line, "NOTIFY * HTTP/1.1") == 0)
            {
                notify = true;
            }
            else if (strcmp(line, "HTTP/1.1 200 OK") == 0)
            {
                search = true;
                notify = true;
            }
            else
            {
                err = true;
                break;
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

    if (ptr != buf)
        free(ptr);
}
