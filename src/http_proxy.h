#ifndef HTTP_PROXY_H
#define HTTP_PROXY_H

typedef struct _http_proxy_t* http_proxy_t;

#include "buf.h"

/* The proxy will convert instances of sourcehost in HTTP headers to
 * targethost. All "converted" data will be written to buf.
 * host is a string of the form "hostname[:port]" if :port is missing, :80 is
 * assumed */
http_proxy_t http_proxy_new(const char* sourcehost, const char* targethost,
                            buf_t output);

void* http_proxy_wptr(http_proxy_t proxy, size_t* avail);
size_t http_proxy_wmove(http_proxy_t proxy, size_t amount);
size_t http_proxy_write(http_proxy_t proxy, const void* data, size_t max);

/* No more data is coming, write what you got to buf.
 * Returns true when all data is transfered to buf (false if buf is full) */
bool http_proxy_flush(http_proxy_t proxy);

void http_proxy_free(http_proxy_t proxy);

#endif /* HTTP_PROXY_H */
