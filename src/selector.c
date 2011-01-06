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
    assert(read_callback != NULL || write_callback != NULL);

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

void selector_chkwrite(selector_t selector, socket_t sock,
                       bool check_write)
{
    if (check_write)
    {
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
            c->read_callback(c->userdata, c->sock);
            c = selector->client + i;
            --ret;
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
            c->write_callback(c->userdata, c->sock);
            c = selector->client + i;
            --ret;
        }
    }

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
        else
        {
            --c;
        }
        --i;
    }

    return true;
}
