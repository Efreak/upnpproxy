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

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "selector.h"
#include "timeval.h"

typedef struct _client_t
{
    socket_t sock;
    void* userdata;
    read_callback_t read_callback;
    write_callback_t write_callback;
    bool delete;
} client_t;

struct _selector_t
{
    fd_set read_set, write_set;
    client_t* client;
    size_t clients, clients_alloc;

    size_t tick_pos, delete_cnt;
    bool in_tick;
};

selector_t selector_new(void)
{
    selector_t ret = calloc(1, sizeof(struct _selector_t));
    if (ret == NULL)
        return ret;

    FD_ZERO(&(ret->read_set));
    FD_ZERO(&(ret->write_set));

    return ret;
}

void selector_free(selector_t selector)
{
    if (selector == NULL)
        return;

    free(selector->client);
    free(selector);
}

void selector_add(selector_t selector, socket_t sock,
                  void* userdata,
                  read_callback_t read_callback,
                  write_callback_t write_callback)
{
    client_t* c;
    size_t i;
    assert(read_callback != NULL || write_callback != NULL);

    for (i = 0, c = selector->client; i < selector->clients; ++i, ++c)
    {
        if (c->sock == sock)
        {
            assert(c->delete);
            --(selector->delete_cnt);
            break;
        }
    }
    if (i == selector->clients)
    {
        if (selector->clients == selector->clients_alloc)
        {
            size_t na = selector->clients_alloc * 2;
            if (na < 4)
                na = 4;
            c = realloc(selector->client, na * sizeof(client_t));
            if (c == NULL)
            {
                na = selector->clients_alloc + 10;
                c = realloc(selector->client, na * sizeof(client_t));
                if (c == NULL)
                {
                    return;
                }
            }
            selector->client = c;
            selector->clients_alloc = na;
        }
        c = selector->client + selector->clients++;
    }
    memset(c, 0, sizeof(client_t));
    c->read_callback = read_callback;
    c->write_callback = write_callback;

    if (read_callback != NULL)
    {
        FD_SET(sock, &selector->read_set);
    }
    if (write_callback != NULL)
    {
        FD_SET(sock, &selector->write_set);
    }
    c->sock = sock;
    c->userdata = userdata;
}

void selector_chk(selector_t selector, socket_t sock,
                  bool check_read, bool check_write)
{
    selector_chkread(selector, sock, check_read);
    selector_chkwrite(selector, sock, check_write);
}

void selector_chkread(selector_t selector, socket_t sock,
                       bool check_read)
{
    if (check_read)
    {
#ifdef DEBUG
        client_t* c;
        size_t i;
        for (i = 0, c = selector->client; i < selector->clients; ++i, ++c)
        {
            if (c->sock == sock)
            {
                assert(c->read_callback != NULL);
                assert(!c->delete);
                break;
            }
        }
        assert(i < selector->clients);
#endif

        FD_SET(sock, &selector->read_set);
    }
    else
    {
        FD_CLR(sock, &selector->read_set);
    }
}

void selector_chkwrite(selector_t selector, socket_t sock,
                       bool check_write)
{
    if (check_write)
    {
#ifdef DEBUG
        client_t* c;
        size_t i;
        for (i = 0, c = selector->client; i < selector->clients; ++i, ++c)
        {
            if (c->sock == sock)
            {
                assert(c->write_callback != NULL);
                assert(!c->delete);
                break;
            }
        }

        assert(i < selector->clients);
#endif

        FD_SET(sock, &selector->write_set);
    }
    else
    {
        FD_CLR(sock, &selector->write_set);
    }
}

void selector_remove(selector_t selector, socket_t sock)
{
    client_t* c;
    size_t i;

    FD_CLR(sock, &selector->read_set);
    FD_CLR(sock, &selector->write_set);

    if (selector->in_tick && selector->client[selector->tick_pos].sock == sock)
    {
        selector->delete_cnt++;
        selector->client[selector->tick_pos].delete = true;
        return;
    }

    for (i = 0, c = selector->client; i < selector->clients; ++i, ++c)
    {
        if (c->sock == sock)
        {
            if (selector->in_tick)
            {
                selector->delete_cnt++;
                c->delete = true;
            }
            else
            {
                selector->clients--;
                memmove(c, c + 1, (selector->clients - i) * sizeof(client_t));
            }
            break;
        }
    }
}

bool selector_tick(selector_t selector, unsigned long timeout_ms)
{
    int ret;
    struct timeval to, target;
    client_t* c;
    size_t i;
    fd_set active_read_set, active_write_set;

    to.tv_sec = timeout_ms / 1000;
    to.tv_usec = (timeout_ms % 1000) * 1000;
    gettimeofday(&target, NULL);
    timeval_add(&target, &to);

    active_read_set = selector->read_set;
    active_write_set = selector->write_set;

    for (;;)
    {
        ret = select(FD_SETSIZE, &active_read_set, &active_write_set, NULL, &to);

        if (ret >= 0)
        {
            break;
        }
        if (errno == EINTR)
        {
            /* We don't have to wait the whole timeout */
            ret = 0;
            break;
        }
        /* Select failed */
        return false;
    }

    selector->in_tick = true;
    selector->delete_cnt = 0;

    for (i = 0, c = selector->client; ret > 0 && i < selector->clients; ++i, ++c)
    {
        if (c->read_callback && FD_ISSET(c->sock, &active_read_set))
        {
            selector->tick_pos = i;
            --ret;
            if (!c->delete)
            {
                c->read_callback(c->userdata, c->sock);
                c = selector->client + i;
            }
            if (ret > 0 && c->write_callback &&
                FD_ISSET(c->sock, &active_write_set))
            {
                --ret;
                if (!c->delete)
                {
                    c->write_callback(c->userdata, c->sock);
                    c = selector->client + i;
                }
            }
        }
        else if (c->write_callback && FD_ISSET(c->sock, &active_write_set))
        {
            selector->tick_pos = i;
            --ret;
            if (!c->delete)
            {
                c->write_callback(c->userdata, c->sock);
                c = selector->client + i;
            }
        }
    }

    assert(ret == 0);

    selector->in_tick = false;

    i = selector->clients;
    c = selector->client + i - 1;
    while (selector->delete_cnt > 0 && i > 0)
    {
        if (c->delete)
        {
            selector->delete_cnt--;
            selector->clients--;
            memmove(c, c + 1, (selector->clients - (i - 1)) * sizeof(client_t));
        }
        --c;
        --i;
    }

#ifdef DEBUG
    for (i = 0, c = selector->client; i < selector->clients; ++i, ++c)
    {
        assert(!c->delete);
    }
#endif

    return true;
}
