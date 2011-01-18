#include "common.h"

#include "http_proxy.h"

#include <stdio.h>
#include <string.h>

#define RUN_TEST(_test) \
    ++tot; cnt += _test ? 1 : 0;

static bool test_http09(void);
static bool test_http10(void);
static bool test_http10_broken(void);
static bool test_http11_closed(void);
static bool test_http11_connlen(void);
static bool test_http11_chunked(void);
static bool test_http11_te(void);
static bool test_req(void);
static bool test_req2(void);
static bool test_req3(void);
static bool test_req4(void);

int main(int argc, char** argv)
{
    unsigned int tot = 0, cnt = 0;

    /* RUN_TEST(test_http09()); */
    RUN_TEST(test_http10());
    RUN_TEST(test_http10_broken());
    RUN_TEST(test_http11_closed());
    RUN_TEST(test_http11_connlen());
    RUN_TEST(test_http11_chunked());
    RUN_TEST(test_http11_te());
    RUN_TEST(test_req());
    RUN_TEST(test_req2());
    RUN_TEST(test_req3());
    RUN_TEST(test_req4());

    fprintf(stdout, "OK %u/%u\n", cnt, tot);

    return cnt == tot ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool got_newlines(const char* str)
{
    return strchr(str, '\n') != NULL || strchr(str, '\r') != NULL;
}

static char* split_lines(char* str, unsigned long* lines)
{
    size_t cnt = 1;
    char* pos = str, *end = str + strlen(str);
    for (;;)
    {
        for (; pos < end && *pos != '\r' && *pos != '\n'; ++pos);
        if (pos == end)
        {
            *lines = cnt;
            return str;
        }
        if (*pos == '\r' && pos[1] == '\n')
        {
            *pos = '\0';
            memmove(pos + 1, pos + 2, (end - (pos + 2)) + 1);
            --end;
            assert(*end == '\0');
        }
        else
        {
            *pos = '\0';
        }
        ++cnt;
        ++pos;
    }
}

static void expected(const char* id, const char* exp, const char* got)
{
    assert(strcmp(exp, got) != 0);
    if (got_newlines(exp) || got_newlines(got))
    {
        unsigned long i, exp2cnt = 0, got2cnt = 0;
        char* expbase = split_lines(strdup(exp), &exp2cnt);
        char* gotbase = split_lines(strdup(got), &got2cnt);
        char* exp2 = expbase, *got2 = gotbase;
        fprintf(stderr, "%s: Line diff\n", id);
        for (i = 0; i < exp2cnt && i < got2cnt; ++i)
        {
            if (strcmp(exp2, got2) != 0)
            {
                fprintf(stderr, "exp %lu: %s\n", i + 1, exp2);
                fprintf(stderr, "got %lu: %s\n", i + 1, got2);
            }
            exp2 += strlen(exp2) + 1;
            got2 += strlen(got2) + 1;
        }
        for (; i < exp2cnt; ++i)
        {
            fprintf(stderr, "exp %lu: %s\n", i + 1, exp2);
            fprintf(stderr, "got %lu: no line\n", i + 1);
            exp2 += strlen(exp2) + 1;
        }
        for (; i < got2cnt; ++i)
        {
            fprintf(stderr, "exp %lu: no line\n", i + 1);
            fprintf(stderr, "got %lu: %s\n", i + 1, got2);
            got2 += strlen(got2) + 1;
        }
        free(expbase);
        free(gotbase);
    }
    else
    {
        fprintf(stderr, "%s: Expected `%s` got `%s`\r\n", id, exp, got);
    }
}

static void append(char** data, size_t *pos, size_t *size, const char* add, size_t len)
{
    if (*pos + len > *size)
    {
        size_t ns = *size * 2;
        if (ns < *pos + len)
        {
            ns = *pos + len;
        }
        if (ns < 100)
        {
            ns = 100;
        }
        *data = realloc(*data, ns);
        *size = ns;
    }
    memcpy(*data + *pos, add, len);
    *pos += len;
}

static void transfer_output(buf_t buf, char** data, size_t *pos, size_t *size)
{
    size_t avail;
    const char* ptr = buf_rptr(buf, &avail);
    if (avail == 0)
    {
        return;
    }
    append(data, pos, size, ptr, avail);
    buf_rmove(buf, avail);
}

static bool test2(const char* id, const char* srchost, const char* tgthost,
                  const char* incoming, char** outgoing)
{
    buf_t output = buf_new(32);
    size_t i, iend;
    size_t osize = 0, o = 0;
    http_proxy_t proxy = http_proxy_new(srchost, tgthost, output);
    *outgoing = NULL;

    iend = strlen(incoming);
    i = 0;
    while (i < iend)
    {
        size_t avail;
        char* ptr = http_proxy_wptr(proxy, &avail);
        if (avail == 0)
        {
            fprintf(stderr, "%s: proxy input buffer full\n", id);
            goto fail;
        }
        if (i + avail > iend)
        {
            avail = iend - i;
        }
        memcpy(ptr, incoming + i, avail);
        http_proxy_wmove(proxy, avail);
        i += avail;
        transfer_output(output, outgoing, &o, &osize);
    }

    while (!http_proxy_flush(proxy))
    {
        size_t o2 = o;
        transfer_output(output, outgoing, &o, &osize);
        if (o2 == o)
        {
            fprintf(stderr, "%s: proxy flush returned false but no data in output to transfer\n", id);
            goto fail;
        }
    }

    while (buf_ravail(output) > 0)
    {
        transfer_output(output, outgoing, &o, &osize);
    }

    buf_free(output);
    http_proxy_free(proxy);
    append(outgoing, &o, &osize, "", 1);
    return true;

 fail:
    buf_free(output);
    http_proxy_free(proxy);
    return false;
}

static bool test(const char* id, const char* srchost, const char* requests,
                 const char* targethost, const char* responses,
                 char** reqs, char** resps)
{
    if (!test2(id, srchost, targethost, requests, reqs))
    {
        return false;
    }
    return test2(id, targethost, srchost, responses, resps);
}

static bool test_http09(void)
{
    const char* source = "source.example.com";
    const char* target = "target.example.com:8080";
    const char* request = "GET /source/index.html\r\n";
    const char* response = "<html><head><title>Meh</title></head>\r\n"
        "<body>meh</body>\r\n"
        "</html>";
    char* req = NULL, *resp = NULL;

    if (!test("http0.9", source, request, target, response, &req, &resp))
    {
        free(req); free(resp);
        return false;
    }

    if (strcmp(request, req) != 0)
    {
        expected("http0.9", request, req);
        free(req); free(resp);
        return false;
    }
    if (strcmp(response, resp) != 0)
    {
        expected("http0.9", response, resp);
        free(req); free(resp);
        return false;
    }

    free(req); free(resp);
    return true;
}

static bool test_http10(void)
{
    const char* source = "source.example.com";
    const char* target = "target.example.com:8080";
    const char* request = "GET /source/index.html HTTP/1.0\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "\r\n";
    const char* response = "HTTP/1.0 200 Resource found ok\r\n"
        "Server: Apache/0.8.4\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><head><title>Meh</title></head>\r\n"
        "<body>meh</body>\r\n"
        "</html>";
    char* req = NULL, *resp = NULL;

    if (!test("http1.0", source, request, target, response, &req, &resp))
    {
        free(req); free(resp);
        return false;
    }

    if (strcmp(request, req) != 0)
    {
        expected("http1.0", request, req);
        free(req); free(resp);
        return false;
    }
    if (strcmp(response, resp) != 0)
    {
        expected("http1.0", response, resp);
        free(req); free(resp);
        return false;
    }

    free(req); free(resp);
    return true;
}

static bool test_http10_broken(void)
{
    const char* source = "source.example.com";
    const char* target = "target.example.com:8080";
    const char* request = "GET /source/index.html HTTP/1.0\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\n"
        "\n";
    const char* response = "\r\nHTTP/1.0 200 Resource found ok\n"
        "Server: Apache/0.8.4\n"
        "Content-Type: text/html\n"
        "\n"
        "<html><head><title>Meh</title></head>\n"
        "<body>meh</body>\n"
        "</html>";
    const char* response_conv = "HTTP/1.0 200 Resource found ok\n"
        "Server: Apache/0.8.4\n"
        "Content-Type: text/html\n"
        "\n"
        "<html><head><title>Meh</title></head>\n"
        "<body>meh</body>\n"
        "</html>";
    char* req = NULL, *resp = NULL;

    if (!test("http1.0_broken", source, request, target, response,
              &req, &resp))
    {
        free(req); free(resp);
        return false;
    }

    if (strcmp(request, req) != 0)
    {
        expected("http1.0_broken", request, req);
        free(req); free(resp);
        return false;
    }
    if (strcmp(response_conv, resp) != 0)
    {
        expected("http1.0_broken", response_conv, resp);
        free(req); free(resp);
        return false;
    }

    free(req); free(resp);
    return true;
}

static bool test_http11_closed(void)
{
    const char* source = "source.example.com";
    const char* target = "target.example.com:8080";
    const char* request1 = "GET /source/index.html HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: source.example.com\r\n"
        "Connection: closed\r\n"
        "\r\n";
    const char* request1_conv = "GET /source/index.html HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: target.example.com:8080\r\n"
        "Connection: closed\r\n"
        "\r\n";
    const char* request2 = "GET /source/index.html HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: target.example.com:8080\r\n"
        "Connection: closed\r\n"
        "\r\n";
    const char* request2_conv = "GET /source/index.html HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: source.example.com\r\n"
        "Connection: closed\r\n"
        "\r\n";
    const char* response = "HTTP/1.1 200 Resource found ok\r\n"
        "Server: Apache/0.8.4\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><head><title>Meh</title></head>\r\n"
        "<body>meh</body>\r\n"
        "</html>";
    char* req = NULL, *resp = NULL;

    if (!test("http1.1_closed:1", source, request1,
              target, response, &req, &resp))
    {
        free(req); free(resp);
        return false;
    }

    if (strcmp(request1_conv, req) != 0)
    {
        expected("http1.1_closed:1", request1_conv, req);
        free(req); free(resp);
        return false;
    }
    if (strcmp(response, resp) != 0)
    {
        expected("http1.1_closed", response, resp);
        free(req); free(resp);
        return false;
    }

    free(req); free(resp);

    if (!test("http1.1_closed:2", target, request2,
              source, response, &req, &resp))
    {
        free(req); free(resp);
        return false;
    }

    if (strcmp(request2_conv, req) != 0)
    {
        expected("http1.1_closed:2", request2_conv, req);
        free(req); free(resp);
        return false;
    }
    if (strcmp(response, resp) != 0)
    {
        expected("http1.1_closed", response, resp);
        free(req); free(resp);
        return false;
    }

    free(req); free(resp);
    return true;
}

static bool test_http11_connlen(void)
{
    const char* source = "source.example.com";
    const char* target = "target.example.com:8080";
    const char* requests = "GET /source/index.html HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: source.example.com\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "DUH\r\n"
        "GET /meh/aaarg%20/file.htm HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: source.example.com\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    const char* requests_conv = "GET /source/index.html HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: target.example.com:8080\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "DUH\r\n"
        "GET /meh/aaarg%20/file.htm HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: target.example.com:8080\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    const char* responses = "HTTP/1.1 200 Resource found ok\r\n"
        "Server: Apache/0.8.4\r\n"
        "Content-Type: text/html\r\n"
        "Content-length: 64\r\n"
        "\r\n"
        "<html><head><title>Meh</title></head>\r\n"
        "<body>meh</body>\r\n"
        "</html>"
        "HTTP/1.1 404 Not found\r\n"
        "Server: Apache/0.8.4\r\n"
        "Content-Type: text/html\r\n"
        "Content-length: 12\r\n"
        "\r\n"
        "Not found!\r\n";
    char* req = NULL, *resp = NULL;

    if (!test("http1.1_connlen", source, requests,
              target, responses, &req, &resp))
    {
        free(req); free(resp);
        return false;
    }

    if (strcmp(requests_conv, req) != 0)
    {
        expected("http1.1_connlen", requests_conv, req);
        free(req); free(resp);
        return false;
    }
    if (strcmp(responses, resp) != 0)
    {
        expected("http1.1_connlen", responses, resp);
        free(req); free(resp);
        return false;
    }

    free(req); free(resp);
    return true;
}

static bool test_http11_chunked(void)
{
    const char* source = "source.example.com";
    const char* target = "target.example.com:8080";
    const char* requests = "GET /source/index.html HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: source.example.com\r\n"
        "TE: chunked\r\n"
        "\r\n"
        "GET /meh/aaarg%20/file.htm HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: source.example.com\r\n"
        "\r\n";
    const char* requests_conv = "GET /source/index.html HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: target.example.com:8080\r\n"
        "TE: chunked\r\n"
        "\r\n"
        "GET /meh/aaarg%20/file.htm HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: target.example.com:8080\r\n"
        "\r\n";
    const char* responses = "HTTP/1.1 200 Resource found ok\r\n"
        "Server: Apache/0.8.4\r\n"
        "Content-Type: text/html\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "040\r\n<html><head><title>Meh</title></head>\r\n"
        "<body>meh</body>\r\n"
        "</html>\r\n"
        "0\r\n\r\n"
        "HTTP/1.1 404 Not found\r\n"
        "Server: Apache/0.8.4\r\n"
        "Content-Type: text/html\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "c\r\nNot found!\r\n\r\n"
        "0\r\n\r\n";
    char* req = NULL, *resp = NULL;

    if (!test("http1.1_chunked", source, requests,
              target, responses, &req, &resp))
    {
        free(req); free(resp);
        return false;
    }

    if (strcmp(requests_conv, req) != 0)
    {
        expected("http1.1_chunked", requests_conv, req);
        free(req); free(resp);
        return false;
    }
    if (strcmp(responses, resp) != 0)
    {
        expected("http1.1_chunked", responses, resp);
        free(req); free(resp);
        return false;
    }

    free(req); free(resp);
    return true;
}

static bool test_http11_te(void)
{
    const char* source = "source.example.com:8080";
    const char* target = "target.example.com";
    const char* request = "GET /source/index.html HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: source.example.com:8080\r\n"
        "TE: trailers\r\n"
        "\r\n";
    const char* request_conv = "GET /source/index.html HTTP/1.1\r\n"
        "User-Agent: CERN-LineMode/2.15 libwww/2.17b3\r\n"
        "Host: target.example.com\r\n"
        "TE: \r\n"
        "\r\n";
    const char* response = "HTTP/1.1 200 Resource found ok\r\n"
        "Server: Apache/0.8.4\r\n"
        "Content-Type: text/html\r\n"
        "Connection: closed\r\n"
        "\r\n"
        "<html><head><title>Meh</title></head>\r\n"
        "<body>meh</body>\r\n"
        "</html>";
    char* req = NULL, *resp = NULL;

    if (!test("http1.1_te", source, request, target, response, &req, &resp))
    {
        free(req); free(resp);
        return false;
    }

    if (strcmp(request_conv, req) != 0)
    {
        expected("http1.1_te", request_conv, req);
        free(req); free(resp);
        return false;
    }
    if (strcmp(response, resp) != 0)
    {
        expected("http1.1_te", response, resp);
        free(req); free(resp);
        return false;
    }

    free(req); free(resp);
    return true;
}

static bool test_req(void)
{
    const char* source = "10.0.1.1:60582";
    const char* target = "192.168.1.7:45000";
    const char* request =
        "GET /5d4799e5-7d24-4bd6-ab2c-76298425d598/description-2.xml HTTP/1.1\r\n"
        "User-Agent: Opera/9.80 (Windows NT 6.1; U; en) Presto/2.7.62 Version/11.00\r\n"
        "Host: 10.0.1.1:60582\r\n"
        "Accept: text/html, application/xml;q=0.9, application/xhtml+xml, image/png, image/jpeg, image/gif, image/x-xbitmap, */*;q=0.1\r\n"
        "Accept-Language: sv-SE,sv;q=0.9,en;q=0.8\r\n"
        "Accept-Charset: iso-8859-1, utf-8, utf-16, *;q=0.1\r\n"
        "Accept-Encoding: deflate, gzip, x-gzip, identity, *;q=0\r\n"
        "Connection: Keep-Alive\r\n"
        "\r\n";
    const char* request_conv =
        "GET /5d4799e5-7d24-4bd6-ab2c-76298425d598/description-2.xml HTTP/1.1\r\n"
        "User-Agent: Opera/9.80 (Windows NT 6.1; U; en) Presto/2.7.62 Version/11.00\r\n"
        "Host: 192.168.1.7:45000\r\n"
        "Accept: text/html, application/xml;q=0.9, application/xhtml+xml, image/png, image/jpeg, image/gif, image/x-xbitmap, */*;q=0.1\r\n"
        "Accept-Language: sv-SE,sv;q=0.9,en;q=0.8\r\n"
        "Accept-Charset: iso-8859-1, utf-8, utf-16, *;q=0.1\r\n"
        "Accept-Encoding: deflate, gzip, x-gzip, identity, *;q=0\r\n"
        "Connection: Keep-Alive\r\n"
        "\r\n";

    char* req = NULL;

    if (!test2("req", source, target, request, &req))
    {
        free(req);
        return false;
    }

    if (strcmp(request_conv, req) != 0)
    {
        expected("req", request_conv, req);
        free(req);
        return false;
    }

    free(req);
    return true;
}

static bool test_req2(void)
{
    const char* request =
        "GET /5d4799e5-7d24-4bd6-ab2c-76298425d598/description-2.xml HTTP/1.1\r\n"
        "User-Agent: Opera/9.80 (Windows NT 6.1; U; en) Presto/2.7.62 Version/11.00\r\n"
        "Host: 10.0.1.1:60582\r\n"
        "Accept: text/html, application/xml;q=0.9, application/xhtml+xml, image/png, image/jpeg, image/gif, image/x-xbitmap, */*;q=0.1\r\n"
        "Accept-Language: sv-SE,sv;q=0.9,en;q=0.8\r\n"
        "Accept-Charset: iso-8859-1, utf-8, utf-16, *;q=0.1\r\n"
        "Accept-Encoding: deflate, gzip, x-gzip, identity, *;q=0\r\n"
        "Connection: Keep-Alive\r\n"
        "\r\n";

    char* req = NULL;

    if (!test2("req", "", "", request, &req))
    {
        free(req);
        return false;
    }

    if (strcmp(request, req) != 0)
    {
        expected("req", request, req);
        free(req);
        return false;
    }

    free(req);
    return true;
}

static bool test_req3(void)
{
    const char* request =
        "GET /5d4799e5-7d24-4bd6-ab2c-76298425d598/ HTTP/1.1\r\n"
        "User-Agent: Opera/9.80 (Windows NT 6.1; U; en) Presto/2.7.62 Version/11.00\r\n"
        "Host: 10.0.1.1:48383\r\n"
        "Accept: text/html, application/xml;q=0.9, application/xhtml+xml, image/png, image/jpeg, image/gif, image/x-xbitmap, */*;q=0.1\r\n"
        "Accept-Language: sv-SE,sv;q=0.9,en;q=0.8\r\n"
        "Accept-Charset: iso-8859-1, utf-8, utf-16, *;q=0.1\r\n"
        "Accept-Encoding: deflate, gzip, x-gzip, identity, *;q=0\r\n"
        "Connection: Keep-Alive, TE\r\n"
        "TE: deflate, gzip, chunked, identity, trailers\r\n"
        "\r\n";
    const char* request_conv =
        "GET /5d4799e5-7d24-4bd6-ab2c-76298425d598/ HTTP/1.1\r\n"
        "User-Agent: Opera/9.80 (Windows NT 6.1; U; en) Presto/2.7.62 Version/11.00\r\n"
        "Host: 192.168.1.7:45000\r\n"
        "Accept: text/html, application/xml;q=0.9, application/xhtml+xml, image/png, image/jpeg, image/gif, image/x-xbitmap, */*;q=0.1\r\n"
        "Accept-Language: sv-SE,sv;q=0.9,en;q=0.8\r\n"
        "Accept-Charset: iso-8859-1, utf-8, utf-16, *;q=0.1\r\n"
        "Accept-Encoding: deflate, gzip, x-gzip, identity, *;q=0\r\n"
        "Connection: Keep-Alive, TE\r\n"
        "TE: deflate, gzip, chunked, identity\r\n"
        "\r\n";
    char* req = NULL;

    if (!test2("req", "10.0.1.1:48383", "192.168.1.7:45000", request, &req))
    {
        free(req);
        return false;
    }

    if (strcmp(request_conv, req) != 0)
    {
        expected("req", request_conv, req);
        free(req);
        return false;
    }

    free(req);
    return true;
}

static bool test_req4(void)
{
    const char* request =
        "GET /5d4799e5-7d24-4bd6-ab2c-76298425d598/ HTTP/1.1\r\n"
        "User-Agent: Opera/9.80 (Windows NT 6.1; U; en) Presto/2.7.62 Version/11.00\r\n"
        "Host: 10.0.1.1:48383\r\n"
        "Accept: text/html, application/xml;q=0.9, application/xhtml+xml, image/png, image/jpeg, image/gif, image/x-xbitmap, */*;q=0.1\r\n"
        "Accept-Language: sv-SE,sv;q=0.9,en;q=0.8\r\n"
        "Accept-Charset: iso-8859-1, utf-8, utf-16, *;q=0.1\r\n"
        "Accept-Encoding: deflate, gzip, x-gzip, identity, *;q=0\r\n"
        "Connection: Keep-Alive, TE\r\n"
        "TE: deflate, gzip, trailers, chunked, identity\r\n"
        "\r\n";
    const char* request_conv =
        "GET /5d4799e5-7d24-4bd6-ab2c-76298425d598/ HTTP/1.1\r\n"
        "User-Agent: Opera/9.80 (Windows NT 6.1; U; en) Presto/2.7.62 Version/11.00\r\n"
        "Host: 192.168.1.7:45000\r\n"
        "Accept: text/html, application/xml;q=0.9, application/xhtml+xml, image/png, image/jpeg, image/gif, image/x-xbitmap, */*;q=0.1\r\n"
        "Accept-Language: sv-SE,sv;q=0.9,en;q=0.8\r\n"
        "Accept-Charset: iso-8859-1, utf-8, utf-16, *;q=0.1\r\n"
        "Accept-Encoding: deflate, gzip, x-gzip, identity, *;q=0\r\n"
        "Connection: Keep-Alive, TE\r\n"
        "TE: deflate, gzip, chunked, identity\r\n"
        "\r\n";
    char* req = NULL;

    if (!test2("req", "10.0.1.1:48383", "192.168.1.7:45000", request, &req))
    {
        free(req);
        return false;
    }

    if (strcmp(request_conv, req) != 0)
    {
        expected("req", request_conv, req);
        free(req);
        return false;
    }

    free(req);
    return true;
}
