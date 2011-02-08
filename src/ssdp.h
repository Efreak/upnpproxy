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

#ifndef SSDP_H
#define SSDP_H

#include "socket.h"
#include "selector.h"
#include "timers.h"
#include "log.h"
#include <time.h>

typedef struct _ssdp_t* ssdp_t;

typedef struct _ssdp_search_t
{
    struct sockaddr* host;
    socklen_t hostlen;
    struct sockaddr* sender;
    socklen_t senderlen;
    char* s, * st;
    unsigned int mx;
} ssdp_search_t;

typedef struct _ssdp_notify_t
{
    struct sockaddr* host;
    socklen_t hostlen;
    char* location;
    char* server;
    char* usn;
    time_t expires;
    char* nt;
    char* nts;
    char* opt, *nls;
} ssdp_notify_t;

typedef void (* ssdp_search_callback_t)(void* userdata, ssdp_search_t* search);
typedef void (* ssdp_search_response_callback_t)(void* userdata, ssdp_search_t* search, ssdp_notify_t* notify);
typedef void (* ssdp_notify_callback_t)(void* userdata, ssdp_notify_t* notify);

ssdp_t ssdp_new(log_t log,
                selector_t selector,
                timers_t timers,
                const char* bindaddr,
                void* userdata,
                ssdp_search_callback_t search_callback,
                ssdp_search_response_callback_t search_response_callback,
                ssdp_notify_callback_t notify_callback);

struct sockaddr* ssdp_getnotifyhost(ssdp_t ssdp, socklen_t* hostlen);

bool ssdp_search(ssdp_t ssdp, ssdp_search_t* search);
bool ssdp_search_response(ssdp_t ssdp, ssdp_search_t* search,
                          ssdp_notify_t* notify);
bool ssdp_notify(ssdp_t ssdp, ssdp_notify_t* notify);
/* Only host, nt and usn members need to be filled */
bool ssdp_byebye(ssdp_t ssdp, ssdp_notify_t* notify);

void ssdp_free(ssdp_t ssdp);

#endif /* SSDP_H */
