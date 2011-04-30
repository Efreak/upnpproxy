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

#include "socket.h"

#ifdef _WIN32
#else
#include <fcntl.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <limits.h>

#include <errno.h>

#ifndef MAX
# define MAX(__x, __y) (((__x) > (__y)) ? (__x) : (__y))
#endif

#if !HAVE_INET_NTOP
const char* rpl_inet_ntop(int af, const void* src, char* dst, size_t size)
{
    if (af == AF_INET)
    {
        char* ret = inet_ntoa(*((const struct in_addr *)src));
        size_t retlen = strlen(ret);
        if (size > 0)
        {
            memcpy(dst, ret, retlen < size ? retlen + 1 : size);
            dst[size - 1] = '\0';
        }
        return dst;
    }
    return NULL;
}
#dst inet_ntop rpl_inet_ntop
#endif

#if !HAVE_INET_PTON
int rpl_inet_pton(int af, const char* src, void* dst)
{
    if (af == AF_INET)
    {
        return inet_aton(str, (struct in_addr *)dst);
    }
    return 0;
}
#define inet_pton rpl_inet_pton
#endif

const char* IPV4_ANY = "IPV4";
const char* IPV6_ANY = "IPV6";

static int domain(int family)
{
    if (family == AF_INET)
    {
        return PF_INET;
    }
#if HAVE_INET6
    if (family == AF_INET6)
    {
        return PF_INET6;
    }
#endif
    return 0;
}

struct sockaddr* socket_allocate_addrbuffer(socklen_t* size)
{
#if HAVE_INET6
    socklen_t s = MAX(sizeof(struct sockaddr_in), sizeof(struct sockaddr_in6));
#else
    socklen_t s = sizeof(struct sockaddr_in);
#endif
    if (size != NULL) *size = s;
    return calloc(1, s);
}

struct sockaddr* parse_addr(const char* addr, uint16_t port, socklen_t *addrlen,
                            bool allow_dnslookup)
{
    if (addr == NULL)
    {
        return NULL;
    }
    if (addr == IPV4_ANY || strcmp(addr, IPV4_ANY) == 0)
    {
        struct sockaddr_in* a = calloc(1, sizeof(struct sockaddr_in));
        if (a == NULL)
            return NULL;
        a->sin_family = AF_INET;
        a->sin_addr.s_addr = htonl(INADDR_ANY);
        a->sin_port = htons(port);
        if (addrlen != NULL) *addrlen = sizeof(struct sockaddr_in);
        return (struct sockaddr*)a;
    }
#if HAVE_INET6
    if (addr == IPV6_ANY || strcmp(addr, IPV6_ANY) == 0)
    {
        struct sockaddr_in6* a = calloc(1, sizeof(struct sockaddr_in6));
        if (a == NULL)
            return NULL;
        a->sin6_family = AF_INET6;
        a->sin6_addr = in6addr_any;
        a->sin6_port = htons(port);
        if (addrlen != NULL) *addrlen = sizeof(struct sockaddr_in6);
        return (struct sockaddr*)a;
    }
    {
        void* data = socket_allocate_addrbuffer(NULL);
        if (data == NULL)
            return NULL;
        if (inet_pton(AF_INET6, addr,
                      &(((struct sockaddr_in6*)data)->sin6_addr)) != 0)
        {
            struct sockaddr_in6* a = data;
            a->sin6_family = AF_INET6;
            a->sin6_port = htons(port);
            if (addrlen != NULL) *addrlen = sizeof(struct sockaddr_in6);
            return data;
        }
        if (inet_pton(AF_INET, addr,
                      &(((struct sockaddr_in*)data)->sin_addr)) != 0)
        {
            struct sockaddr_in* a = data;
            a->sin_family = AF_INET;
            a->sin_port = htons(port);
            if (addrlen != NULL) *addrlen = sizeof(struct sockaddr_in);
            return data;
        }
        if (allow_dnslookup)
        {
            struct hostent* host = gethostbyname2(addr, AF_INET6);
            if (host != NULL)
            {
                struct sockaddr_in6* a = calloc(1, sizeof(struct sockaddr_in6));
                if (a == NULL)
                    return NULL;
                a->sin6_family = AF_INET6;
                memcpy(&(a->sin6_addr), host->h_addr, sizeof(struct in6_addr));
                a->sin6_port = htons(port);
                if (addrlen != NULL) *addrlen = sizeof(struct sockaddr_in6);
                return (struct sockaddr*)a;
            }
            host = gethostbyname2(addr, AF_INET);
            if (host != NULL)
            {
                struct sockaddr_in* a = calloc(1, sizeof(struct sockaddr_in));
                if (a == NULL)
                    return NULL;
                a->sin_family = AF_INET;
                memcpy(&(a->sin_addr), host->h_addr, sizeof(struct in_addr));
                a->sin_port = htons(port);
                if (addrlen != NULL) *addrlen = sizeof(struct sockaddr_in);
                return (struct sockaddr*)a;
            }
        }
    }
#else
    {
        struct in_addr data;
        if (inet_pton(AF_INET, addr, &data) != 0)
        {
            struct sockaddr_in* a = calloc(1, sizeof(struct sockaddr_in));
            if (a == NULL)
                return NULL;
            a->sin_family = AF_INET;
            a->sin_addr = data;
            a->sin_port = htons(port);
            if (addrlen != NULL) *addrlen = sizeof(struct sockaddr_in);
            return (struct sockaddr*)a;
        }
        if (allow_dnslookup)
        {
            struct hostent* host = gethostbyname(addr);
            if (host != NULL && host->h_addrtype == AF_INET)
            {
                struct sockaddr_in* a = calloc(1, sizeof(struct sockaddr_in));
                if (a == NULL)
                    return NULL;
                a->sin_family = AF_INET;
                memcpy(&(a->sin_addr), host->h_addr, sizeof(struct in_addr));
                a->sin_port = htons(port);
                if (addrlen != NULL) *addrlen = sizeof(struct sockaddr_in);
                return (struct sockaddr*)a;
            }
        }
    }
#endif
    return NULL;
}

static socket_t socket_listen2(int type, socket_t presock,
                               const struct sockaddr* addr, socklen_t addrlen)
{
    socket_t sock;
    if (presock >= 0)
    {
        sock = presock;
    }
    else
    {
        sock = socket(domain(addr->sa_family), type, 0);
        if (sock < 0)
        {
            return -1;
        }
    }
    {
        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    }
    if (bind(sock, addr, addrlen))
    {
        close(sock);
        return -1;
    }
    if (type == SOCK_STREAM)
    {
        if (listen(sock, 5))
        {
            close(sock);
            return -1;
        }
    }
    return sock;
}

static socket_t socket_listen(int type, const char* bindaddr, uint16_t port)
{
    socklen_t addrlen = 0;
    struct sockaddr* addr;
    socket_t sock = -1;
    if (bindaddr == NULL)
    {
        size_t a;
        for (a = 0; a < 2; ++a)
        {
            addr = parse_addr(a == 0 ? IPV4_ANY : IPV6_ANY, port, &addrlen,
                              false);
            if (addr == NULL)
            {
                continue;
            }
            sock = socket(domain(addr->sa_family), type, 0);
            if (sock < 0)
            {
                free(addr);
                continue;
            }
            break;
        }
        if (a == 2)
        {
            return -1;
        }
    }
    else
    {
        addr = parse_addr(bindaddr, port, &addrlen, true);
        if (addr == NULL)
        {
            return -1;
        }
    }

    sock = socket_listen2(type, sock, addr, addrlen);
    free(addr);
    return sock;
}

static socket_t socket_connect2(int type,
                                const struct sockaddr* addr, socklen_t addrlen,
                                bool block)
{
    socket_t sock;
    sock = socket(domain(addr->sa_family), type, 0);
    if (sock < 0)
    {
        return -1;
    }
    if (!socket_setblocking(sock, block))
    {
        close(sock);
        return -1;
    }
    if (connect(sock, addr, addrlen))
    {
        if (block || errno != EINPROGRESS)
        {
            close(sock);
            return -1;
        }
    }
    return sock;
}

static socket_t socket_connect(int type, const char* host, uint16_t port, bool block)
{
    socklen_t addrlen;
    struct sockaddr* addr = parse_addr(host, port, &addrlen, true);
    socket_t sock;
    if (addr == NULL)
    {
        return -1;
    }
    sock = socket_connect2(type, addr, addrlen, block);
    free(addr);
    return sock;
}

socket_t socket_tcp_listen(const char* bindaddr, uint16_t port)
{
    return socket_listen(SOCK_STREAM, bindaddr, port);
}

socket_t socket_udp_listen(const char* bindaddr, uint16_t port)
{
    return socket_listen(SOCK_DGRAM, bindaddr, port);
}

socket_t socket_tcp_listen2(const struct sockaddr* addr, socklen_t addrlen)
{
    return socket_listen2(SOCK_STREAM, -1, addr, addrlen);
}

socket_t socket_udp_listen2(const struct sockaddr* addr, socklen_t addrlen)
{
    return socket_listen2(SOCK_DGRAM, -1, addr, addrlen);
}

socket_t socket_tcp_connect(const char* host, uint16_t port, bool block)
{
    return socket_connect(SOCK_STREAM, host, port, block);
}

socket_t socket_udp_connect(const char* host, uint16_t port, bool block)
{
    return socket_connect(SOCK_DGRAM, host, port, block);
}

socket_t socket_tcp_connect2(const struct sockaddr* addr, socklen_t addrlen,
                             bool block)
{
    return socket_connect2(SOCK_STREAM, addr, addrlen, block);
}

socket_t socket_udp_connect2(const struct sockaddr* addr, socklen_t addrlen,
                             bool block)
{
    return socket_connect2(SOCK_DGRAM, addr, addrlen, block);
}

socket_t socket_accept(socket_t sock, struct sockaddr** addr, socklen_t* addrlen)
{
    socket_t ret;
    struct sockaddr* tmp;
    socklen_t tmplen;
#if HAVE_INET6
    tmplen = sizeof(struct sockaddr_in6);
    if (sizeof(struct sockaddr_in) > tmplen)
        tmplen = sizeof(struct sockaddr_in);
#else
    tmplen = sizeof(struct sockaddr_in);
#endif
    tmp = malloc(tmplen);
    if (tmp == NULL)
        return -1;
    for (;;)
    {
        ret = accept(sock, tmp, &tmplen);
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (addr != NULL) *addr = NULL;
            if (addrlen != NULL) *addrlen = 0;
            free(tmp);
            return -1;
        }
        break;
    }
    if (addr != NULL) *addr = tmp;
    if (addrlen != NULL) *addrlen = tmplen;
    return ret;
}

static bool socket_multicast(socket_t sock, const char* group,
                             const char* bindaddr, bool join)
{
    struct sockaddr* grp = parse_addr(group, 0, NULL, true);
    struct sockaddr* _bind = parse_addr(bindaddr, 0, NULL, true);
    if (grp == NULL)
    {
        return false;
    }
    if (_bind != NULL && _bind->sa_family != grp->sa_family)
    {
        free(grp);
        free(_bind);
        return false;
    }
    if (grp->sa_family == AF_INET)
    {
        struct sockaddr_in* g = (struct sockaddr_in*)grp;
        struct sockaddr_in* b = (struct sockaddr_in*)_bind;
        struct ip_mreq mreq;
        int ret;
        mreq.imr_multiaddr = g->sin_addr;
        if (b != NULL)
        {
            mreq.imr_interface = b->sin_addr;
        }
        else
        {
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        }
        ret = setsockopt(sock, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
        free(grp);
        free(_bind);
        return ret == 0;
    }
#if HAVE_INET6
    if (grp->sa_family == AF_INET6)
    {
        struct sockaddr_in6* g = (struct sockaddr_in6*)grp;
        struct sockaddr_in6* b = (struct sockaddr_in6*)_bind;
        struct ipv6_mreq mreq;
        int ret;
        mreq.ipv6mr_multiaddr = g->sin6_addr;
        if (b == NULL)
        {
            mreq.ipv6mr_interface = 0;
        }
        else
        {
            /* TODO !!! */
            assert(false);
            mreq.ipv6mr_interface = 0;
            /* b->sin6_addr */
        }
        ret = setsockopt(sock, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
        free(grp);
        free(_bind);
        return ret == 0;
    }
#endif
    free(grp);
    free(_bind);
    return false;
}

bool socket_multicast_join(socket_t sock, const char* group,
                           const char* bindaddr)
{
    return socket_multicast(sock, group, bindaddr, true);
}

bool socket_multicast_drop(socket_t sock, const char* group,
                           const char* bindaddr)
{
    return socket_multicast(sock, group, bindaddr, false);
}

bool socket_multicast_setttl(socket_t sock, unsigned char ttl)
{
    return setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
                      &ttl, sizeof(ttl)) == 0;
}

void socket_close(socket_t sock)
{
    if (sock == -1)
        return;

    close(sock);
}

bool socket_setblocking(socket_t sock, bool blocking)
{
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0)
    {
        return false;
    }
    if (blocking)
    {
        if ((flags & O_NONBLOCK) == 0)
        {
            return true;
        }
        flags &= ~O_NONBLOCK;
    }
    else
    {
        if ((flags & O_NONBLOCK) != 0)
        {
            return true;
        }
        flags |= O_NONBLOCK;
    }
    return fcntl(sock, F_SETFL, flags) == 0;
}

void asprinthost(char** str, const struct sockaddr* addr, socklen_t addrlen)
{
    size_t size, len;
    assert(str != NULL && addr != NULL);
    if (addr->sa_family == AF_INET && addrlen == sizeof(struct sockaddr_in))
    {
        const struct sockaddr_in* a = (const struct sockaddr_in*) addr;
        size = INET_ADDRSTRLEN + 8;
        *str = calloc(1, size);
        inet_ntop(AF_INET, &(a->sin_addr), *str, size);
        len = strlen(*str);
        snprintf(*str + len, size - len, ":%u", ntohs(a->sin_port));
        return;
    }
#if HAVE_INET6
    if (addr->sa_family == AF_INET6 && addrlen == sizeof(struct sockaddr_in6))
    {
        const struct sockaddr_in6* a = (const struct sockaddr_in6*) addr;
        size = INET6_ADDRSTRLEN + 8;
        *str = calloc(1, size);
        inet_ntop(AF_INET6, &(a->sin6_addr), *str, size);
        len = strlen(*str);
        snprintf(*str + len, size - len, ":%u", ntohs(a->sin6_port));
        return;
    }
#endif
    *str = strdup("[unknown]");
}

ssize_t socket_read(socket_t sock, void* data, size_t max)
{
    ssize_t ret;
    for (;;)
    {
        ret = read(sock, data, max);
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
        }
        return ret;
    }
}

ssize_t socket_write(socket_t sock, const void* data, size_t max)
{
    ssize_t ret;
    for (;;)
    {
        ret = write(sock, data, max);
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
        }
        return ret;
    }
}

ssize_t socket_udp_read(socket_t sock, void* data, size_t max,
                        struct sockaddr* addr, socklen_t* addrlen)
{
    ssize_t ret;
    for (;;)
    {
        ret = recvfrom(sock, data, max, 0, addr, addrlen);
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
        }
        return ret;
    }
}

ssize_t socket_udp_write(socket_t sock, const void* data, size_t max,
                         struct sockaddr* addr, socklen_t addrlen)
{
    ssize_t ret;
    for (;;)
    {
        if (addr != NULL)
        {
            ret = sendto(sock, data, max, 0, addr, addrlen);
        }
        else
        {
            ret = send(sock, data, max, 0);
        }
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
        }
        return ret;
    }
}

bool socket_blockingerror(socket_t sock)
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

const char* socket_strerror(socket_t sock)
{
    return strerror(errno);
}

bool addr_is_ipv4(const struct sockaddr* addr, socklen_t addrlen)
{
    return addr != NULL && addrlen == sizeof(struct sockaddr_in) &&
        addr->sa_family == AF_INET;
}

bool addr_is_ipv6(const struct sockaddr* addr, socklen_t addrlen)
{
#if HAVE_INET6
    return addr != NULL && addrlen == sizeof(struct sockaddr_in6) &&
        addr->sa_family == AF_INET6;
#else
    return false;
#endif
}

bool addrstr_is_ipv4(const char* addr)
{
    struct in_addr tmp;
    if (addr == IPV4_ANY || strcmp(addr, IPV4_ANY) == 0)
    {
        return true;
    }
    return inet_pton(AF_INET, addr, &tmp) != 0;
}

bool addrstr_is_ipv6(const char* addr)
{
    if (addr == IPV6_ANY || strcmp(addr, IPV6_ANY) == 0)
    {
        return true;
    }
#if HAVE_INET6
    {
        struct in6_addr tmp;
        return inet_pton(AF_INET6, addr, &tmp) != 0;
    }
#else
    return false;
#endif
}

bool addr_is_any(const struct sockaddr* addr, socklen_t addrlen)
{
    if (addrlen == sizeof(struct sockaddr_in) && addr->sa_family == AF_INET)
    {
        const struct sockaddr_in* a = (const struct sockaddr_in *)addr;
        return a->sin_addr.s_addr == htonl(INADDR_ANY);
    }
#if HAVE_INET6
    if (addrlen == sizeof(struct sockaddr_in6) && addr->sa_family == AF_INET6)
    {
        const struct sockaddr_in6* a = (const struct sockaddr_in6 *)addr;
        return a->sin6_addr.s6_addr == in6addr_any.s6_addr;
    }
#endif
    return false;
}

uint16_t addr_getport(const struct sockaddr* addr, socklen_t addrlen)
{
    if (addrlen == sizeof(struct sockaddr_in) && addr->sa_family == AF_INET)
    {
        const struct sockaddr_in* a = (const struct sockaddr_in *)addr;
        return ntohs(a->sin_port);
    }
#if HAVE_INET6
    if (addrlen == sizeof(struct sockaddr_in6) && addr->sa_family == AF_INET6)
    {
        const struct sockaddr_in6* a = (const struct sockaddr_in6 *)addr;
        return ntohs(a->sin6_port);
    }
#endif
    return 0;
}

void addr_setport(struct sockaddr* addr, socklen_t addrlen, uint16_t newport)
{
    if (addrlen == sizeof(struct sockaddr_in) && addr->sa_family == AF_INET)
    {
        struct sockaddr_in* a = (struct sockaddr_in *)addr;
        a->sin_port = htons(newport);
    }
#if HAVE_INET6
    else if (addrlen == sizeof(struct sockaddr_in6) &&
             addr->sa_family == AF_INET6)
    {
        struct sockaddr_in6* a = (struct sockaddr_in6 *)addr;
        a->sin6_port = htons(newport);
    }
#endif
}

struct sockaddr* socket_getpeeraddr(socket_t sock, socklen_t *addrlen)
{
    socklen_t len;
    struct sockaddr* addr = socket_allocate_addrbuffer(&len);
    if (getpeername(sock, addr, &len))
    {
        free(addr);
        return NULL;
    }
    if (addrlen != NULL)
    {
        *addrlen = len;
    }
    return addr;
}

struct sockaddr* socket_getsockaddr(socket_t sock, socklen_t *addrlen)
{
    socklen_t len;
    struct sockaddr* addr = socket_allocate_addrbuffer(&len);
    if (getsockname(sock, addr, &len))
    {
        free(addr);
        return NULL;
    }
    if (addrlen != NULL)
    {
        *addrlen = len;
    }
    return addr;
}

struct sockaddr* socket_getlocalhost(socket_t sock, uint16_t port,
                                     socklen_t* addrlen)
{
    const char* myname = NULL;
    struct hostent* ent = NULL;
    char buf[HOST_NAME_MAX + 1], *tmp = NULL;
    if (gethostname(buf, sizeof(buf)) == 0)
    {
        myname = buf;
    }
    else
    {
        size_t ns = sizeof(buf);
        for (;;)
        {
            char* tmp2;
            ns *= 2;
            tmp2 = realloc(tmp, ns);
            if (tmp2 == NULL)
            {
                free(tmp);
                myname = "localhost";
                break;
            }
            tmp = tmp2;
            if (gethostname(tmp, ns) == 0)
            {
                myname = tmp;
                break;
            }
        }
    }

    if (sock >= 0)
    {
        socklen_t hostlen;
        struct sockaddr* host = socket_getsockaddr(sock, &hostlen);
        if (host != NULL)
        {
            if (addr_is_ipv4(host, hostlen))
            {
#if HAVE_INET6
                ent = gethostbyname2(myname, AF_INET);
#else
                ent = gethostbyname(myname);
#endif
            }
            else if (addr_is_ipv6(host, hostlen))
            {
#if HAVE_INET6
                ent = gethostbyname2(myname, AF_INET6);
#endif
            }
            free(host);
        }
    }
    else
    {
#if HAVE_INET6
        ent = gethostbyname2(myname, AF_INET6);
        if (ent == NULL)
        {
            ent = gethostbyname2(myname, AF_INET);
        }
#else
        ent = gethostbyname(myname, AF_INET6);
#endif
    }

    if (tmp != NULL)
    {
        free(tmp);
    }

    if (ent != NULL)
    {
#if HAVE_INET6
        if (ent->h_addrtype == AF_INET6)
        {
            struct sockaddr_in6* a = calloc(1, sizeof(struct sockaddr_in6));
            if (a == NULL)
                return NULL;
            a->sin6_family = AF_INET6;
            memcpy(&(a->sin6_addr), ent->h_addr, sizeof(struct in6_addr));
            a->sin6_port = htons(port);
            if (addrlen != NULL) *addrlen = sizeof(struct sockaddr_in6);
            return (struct sockaddr*)a;
        }
#endif
        if (ent->h_addrtype == AF_INET)
        {
            struct sockaddr_in* a = calloc(1, sizeof(struct sockaddr_in));
            if (a == NULL)
                return NULL;
            a->sin_family = AF_INET;
            memcpy(&(a->sin_addr), ent->h_addr, sizeof(struct in_addr));
            a->sin_port = htons(port);
            if (addrlen != NULL) *addrlen = sizeof(struct sockaddr_in);
            return (struct sockaddr*)a;
        }
    }

    /* Should really not happen */
    return parse_addr(IPV4_ANY, port, addrlen, false);
}

bool socket_samehost(const struct sockaddr* a1, socklen_t a1len,
                     const struct sockaddr* a2, socklen_t a2len)
{
    if (a1len != a2len || a1->sa_family != a2->sa_family)
    {
        return false;
    }

    if (a1len == sizeof(struct sockaddr_in) && a1->sa_family == AF_INET)
    {
        return (memcmp(&((const struct sockaddr_in*)a1)->sin_addr,
                       &((const struct sockaddr_in*)a2)->sin_addr,
                       sizeof(struct in_addr)) == 0);
    }

#if HAVE_INET6
    if (a1len == sizeof(struct sockaddr_in6) && a1->sa_family == AF_INET6)
    {
        return (memcmp(&((const struct sockaddr_in6*)a1)->sin6_addr,
                       &((const struct sockaddr_in6*)a2)->sin6_addr,
                       sizeof(struct in6_addr)) == 0);
    }
#endif

    return false;
}

bool socket_samehostandport(const struct sockaddr* a1, socklen_t a1len,
                            const struct sockaddr* a2, socklen_t a2len)
{
    return (a1len == a2len && memcmp(a1, a2, a1len) == 0);
}

#endif
