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
