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

#ifndef HTTP_H
#define HTTP_H

typedef struct _http_req_t* http_req_t;
typedef struct _http_resp_t* http_resp_t;

#include "socket.h"
#include "log.h"

http_req_t req_new(const char* action, const char* url, const char* version);

void req_addheader(http_req_t req, const char* key, const char* value);
void req_addbody(http_req_t req, const char* body);

bool req_send(http_req_t req, socket_t sock,
              struct sockaddr* dst, socklen_t dstlen, log_t log);

void req_free(http_req_t req);

http_resp_t resp_new(unsigned int code, const char* status,
                     const char* version);

void resp_addheader(http_resp_t resp, const char* key, const char* value);
void resp_addbody(http_resp_t resp, const char* body);

bool resp_send(http_resp_t resp, socket_t sock,
               struct sockaddr* dst, socklen_t dstlen, log_t log);

void resp_free(http_resp_t resp);

#endif /* HTTP_H */
