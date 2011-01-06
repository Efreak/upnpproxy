#include "common.h"

#include "http.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct _pkg_t
{
    char* data;
    size_t size, alloc;
    bool got_body;
}* pkg_t;

static void pkg_init(pkg_t pkg, const char* format, ...);
static void pkg_addheader(pkg_t pkg, const char* key, const char* value);
static void pkg_addbody(pkg_t pkg, const char* body);
static bool pkg_send(pkg_t pkg, socket_t sock,
                     struct sockaddr* dst, socklen_t dstlen, log_t log);
static void pkg_free(pkg_t pkg);

struct _http_req_t
{
    struct _pkg_t pkg;
};

http_req_t req_new(const char* action, const char* url, const char* version)
{
    http_req_t req = calloc(1, sizeof(struct _http_req_t));
    if (req == NULL)
        return req;
    pkg_init(&(req->pkg), "%s %s HTTP/%s", action, url, version);
    return req;
}

void req_addheader(http_req_t req, const char* key, const char* value)
{
    pkg_addheader(&(req->pkg), key, value);
}

void req_addbody(http_req_t req, const char* body)
{
    pkg_addbody(&(req->pkg), body);
}

bool req_send(http_req_t req, socket_t sock,
              struct sockaddr* dst, socklen_t dstlen, log_t log)
{
    return pkg_send(&(req->pkg), sock, dst, dstlen, log);
}

void req_free(http_req_t req)
{
    pkg_free(&(req->pkg));
    free(req);
}

struct _http_resp_t
{
    struct _pkg_t pkg;
};

http_resp_t resp_new(unsigned int code, const char* status,
                     const char* version)
{
    http_resp_t resp = calloc(1, sizeof(struct _http_resp_t));
    if (resp == NULL)
        return resp;
    pkg_init(&(resp->pkg), "HTTP/%s %u %s", version, code, status);
    return resp;
}

void resp_addheader(http_resp_t resp, const char* key, const char* value)
{
    pkg_addheader(&(resp->pkg), key, value);
}

void resp_addbody(http_resp_t resp, const char* body)
{
    pkg_addbody(&(resp->pkg), body);
}

bool resp_send(http_resp_t resp, socket_t sock, struct sockaddr* dst, socklen_t dstlen, log_t log)
{
    return pkg_send(&(resp->pkg), sock, dst, dstlen, log);
}

void resp_free(http_resp_t resp)
{
    pkg_free(&(resp->pkg));
    free(resp);
}

static void pkg_append(pkg_t pkg, const char* str)
{
    size_t len = strlen(str);
    if (pkg->size + len >= pkg->alloc)
    {
        size_t na = pkg->alloc * 2;
        char* tmp;
        if (na < 512)
            na = 512;
        tmp = realloc(pkg->data, na);
        if (tmp == NULL)
        {
            na = pkg->size + len + 256;
            tmp = realloc(pkg->data, na);
            if (tmp == NULL)
                return;
        }
        pkg->alloc = na;
        pkg->data = tmp;
    }
    memcpy(pkg->data + pkg->size, str, len);
    pkg->size += len;
}

void pkg_init(pkg_t pkg, const char* format, ...)
{
    va_list args;
    char* tmp;
    va_start(args, format);
    vasprintf(&tmp, format, args);
    va_end(args);
    pkg_append(pkg, tmp);
    free(tmp);
    pkg_append(pkg, "\r\n");
}

void pkg_addheader(pkg_t pkg, const char* key, const char* value)
{
    assert(strchr(key, ':') == NULL &&
           strchr(key, '\n') == NULL &&
           strchr(value, '\n') == NULL);
    pkg_append(pkg, key);
    pkg_append(pkg, ": ");
    pkg_append(pkg, value);
    pkg_append(pkg, "\r\n");
}

void pkg_addbody(pkg_t pkg, const char* body)
{
    if (!pkg->got_body)
    {
        pkg_append(pkg, "\r\n");
        pkg->got_body = true;
    }
    pkg_append(pkg, body);
}

bool pkg_send(pkg_t pkg, socket_t sock, struct sockaddr* dst, socklen_t dstlen, log_t log)
{
    size_t pos = 0;
    ssize_t sent;
    if (!pkg->got_body)
    {
        pkg_append(pkg, "\r\n");
    }
    while (pos < pkg->size)
    {
        sent = socket_udp_write(sock, pkg->data + pos, pkg->size - pos,
                                dst, dstlen);
        if (sent <= 0)
        {
            log_printf(log, LVL_WARN,
                       "Unable to send package: %s", socket_strerror(sock));
            return false;
        }
        pos += sent;
    }
    return true;
}

void pkg_free(pkg_t pkg)
{
    free(pkg->data);
}
