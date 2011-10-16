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

#ifndef SOCKET_H
#define SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>

#ifdef _WIN32
typedef int socklen_t;

#else

typedef int socket_t;

#endif

extern const char* IPV4_ANY;
extern const char* IPV6_ANY;

socket_t socket_tcp_listen(const char* bindaddr, uint16_t port);
socket_t socket_udp_listen(const char* bindaddr, uint16_t port);

socket_t socket_tcp_listen2(const struct sockaddr* addr, socklen_t addrlen);
socket_t socket_udp_listen2(const struct sockaddr* addr, socklen_t addrlen);

socket_t socket_tcp_connect(const char* host, uint16_t port, bool block,
                            const char* bindaddr);
socket_t socket_udp_connect(const char* host, uint16_t port, bool block,
                            const char* bindaddr);

socket_t socket_tcp_connect2(const struct sockaddr* addr, socklen_t addrlen,
                             bool block, const char* bindaddr);
socket_t socket_udp_connect2(const struct sockaddr* addr, socklen_t addrlen,
                             bool block, const char* bindaddr);

socket_t socket_accept(socket_t sock,
                       struct sockaddr** addr, socklen_t* addrlen);

bool socket_multicast_join(socket_t sock, const char* group,
                           const char* bindaddr);
bool socket_multicast_drop(socket_t sock, const char* group,
                           const char* bindaddr);
bool socket_multicast_setttl(socket_t sock, unsigned char ttl);

ssize_t socket_read(socket_t sock, void* data, size_t max);
ssize_t socket_write(socket_t sock, const void* data, size_t max);

/* Addr may be NULL */
ssize_t socket_udp_read(socket_t sock, void* data, size_t max,
                        struct sockaddr* addr, socklen_t* addrlen);
/* Addr may be NULL */
ssize_t socket_udp_write(socket_t sock, const void* data, size_t max,
                         struct sockaddr* addr, socklen_t addrlen);

struct sockaddr* socket_allocate_addrbuffer(socklen_t* size);

bool socket_blockingerror(socket_t sock);
/* sock may be -1 as we might want the error causing the socket not to be
 * created */
const char* socket_strerror(socket_t sock);

void socket_close(socket_t sock);

bool socket_setblocking(socket_t sock, bool blocking);

void asprinthost(char** str, const struct sockaddr* addr, socklen_t addrlen);

bool addr_is_ipv4(const struct sockaddr* addr, socklen_t addrlen);
bool addr_is_ipv6(const struct sockaddr* addr, socklen_t addrlen);
bool addrstr_is_ipv4(const char* addr);
bool addrstr_is_ipv6(const char* addr);

/* Any as in 0.0.0.0 or :: */
bool addr_is_any(const struct sockaddr* addr, socklen_t addrlen);

uint16_t addr_getport(const struct sockaddr* addr, socklen_t addrlen);
void addr_setport(struct sockaddr* addr, socklen_t addrlen, uint16_t newport);

struct sockaddr* socket_getpeeraddr(socket_t sock, socklen_t *addrlen);
struct sockaddr* socket_getsockaddr(socket_t sock, socklen_t *addrlen);

/* The socket is only used to select between an IPv4 or IPv6 address
 * if available. May be < 0 and is then ignored. */
struct sockaddr* socket_getlocalhost(socket_t sock, uint16_t port,
                                     socklen_t* addrlen);

struct sockaddr* parse_addr(const char* addr, uint16_t port,
                            socklen_t *addrlen, bool allow_dnslookup);

bool socket_samehost(const struct sockaddr* a1, socklen_t a1len,
                     const struct sockaddr* a2, socklen_t a2len);
bool socket_samehostandport(const struct sockaddr* a1, socklen_t a1len,
                            const struct sockaddr* a2, socklen_t a2len);

#endif /* SOCKET_H */
