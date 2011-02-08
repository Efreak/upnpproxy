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

#ifndef SELECTOR_H
#define SELECTOR_H

typedef struct _selector_t* selector_t;

#include "socket.h"

typedef void (* read_callback_t)(void* userdata, socket_t sock);
typedef void (* write_callback_t)(void* userdata, socket_t sock);

selector_t selector_new(void);

void selector_add(selector_t selector, socket_t sock,
                  void* userdata,
                  read_callback_t read_callback,
                  write_callback_t write_callback);

/* This function does not check if you set a write_callback back when calling
 * add. If you set check_write to true and then have write_callback == NULL
 * you just caused a segfault. */
void selector_chk(selector_t selector, socket_t sock,
                  bool check_read, bool check_write);
void selector_chkread(selector_t selector, socket_t sock,
                      bool check_read);
void selector_chkwrite(selector_t selector, socket_t sock,
                       bool check_write);

void selector_remove(selector_t selector, socket_t sock);

void selector_free(selector_t selector);

/* timeout_ms == 0 means no timeout */
bool selector_tick(selector_t selector, unsigned long timeout_ms);

#endif /* SELECTOR_H */
