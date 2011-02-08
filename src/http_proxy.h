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

/* force == true : No more data is coming, write what you got to buf.
 * Returns true when all data is transfered to buf (false if buf is full) */
bool http_proxy_flush(http_proxy_t proxy, bool force);

void http_proxy_free(http_proxy_t proxy);

#endif /* HTTP_PROXY_H */
