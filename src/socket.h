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

socket_t socket_tcp_connect(const char* host, uint16_t port, bool block);
socket_t socket_udp_connect(const char* host, uint16_t port, bool block);

socket_t socket_tcp_connect2(const struct sockaddr* addr, socklen_t addrlen,
                             bool block);
socket_t socket_udp_connect2(const struct sockaddr* addr, socklen_t addrlen,
                             bool block);

socket_t socket_accept(socket_t sock,
                       struct sockaddr** addr, socklen_t* addrlen);

bool socket_multicast_join(socket_t sock, const char* group,
                           const char* bindaddr);
bool socket_multicast_drop(socket_t sock, const char* group,
                           const char* bindaddr);
bool socket_multicast_setttl(socket_t sock, unsigned char ttl);

ssize_t socket_read(socket_t sock, void* data, size_t max);
ssize_t socket_write(socket_t sock, const void* data, size_t max);

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

void addr_setport(struct sockaddr* addr, socklen_t addrlen, uint16_t newport);

struct sockaddr* socket_getpeeraddr(socket_t sock, socklen_t *addrlen);
struct sockaddr* socket_getsockaddr(socket_t sock, socklen_t *addrlen);

struct sockaddr* parse_addr(const char* addr, uint16_t port,
                            socklen_t *addrlen, bool allow_dnslookup);

bool socket_samehost(const struct sockaddr* a1, socklen_t a1len,
                     const struct sockaddr* a2, socklen_t a2len);
bool socket_samehostandport(const struct sockaddr* a1, socklen_t a1len,
                            const struct sockaddr* a2, socklen_t a2len);

#endif /* SOCKET_H */
