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
    uint mx;
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
