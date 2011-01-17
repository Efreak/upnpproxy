#include "common.h"

#include "http_proxy.h"
#include "buf.h"

#include <string.h>
#include <limits.h>
#include <errno.h>

#ifdef DEBUG
# include <stdio.h>
#endif

typedef enum _state_t
{
    STATE_SEASON4 = 0, /* Before dawn, first ever call to proxy_flush
                        * -  init iterators */
    STATE_DAWN,        /* Not even received Request-Line or Status-Line */
    STATE_HEADER,      /* Receiving headers */
    STATE_BODY,        /* Receiving body */
} state_t;

typedef struct _iter_t
{
    const char* ptr, *pos, *end;
} iter_t;

struct _http_proxy_t
{
    char* sourcehost;
    char* targethost;

    buf_t input;
    buf_t output;

    iter_t last;
    char* tmpstr;
    size_t tmplen;

    bool active_transfer;
    size_t transfer_left;

    bool active_replace;
    const char* replace;
    size_t replace_skip;

    uint64_t content_pos;

    bool in_chunk;
    uint64_t chunk_size, chunk_pos;

    state_t state;
    /* state >= STATE_HEADER */
    bool request; /* true if the current message is a request,
                   * false if its a response */
    unsigned int response_code; /* only set if request is false */
    unsigned int major, minor; /* major and minor http version */
    /* state >= STATE_BODY */
    bool content_length_set;
    uint64_t content_length;
    bool chunked;
    bool closed; /* true if the response will be terminated by
                  * connection close */
};

/* HTTP/1.0
    Full-Request   = Request-Line             ; Section 5.1
                     *( General-Header        ; Section 4.3
                      | Request-Header        ; Section 5.2
                      | Entity-Header )       ; Section 7.1
                      CRLF
                     [ Entity-Body ]          ; Section 7.2

    Request-Line = Method SP Request-URI SP HTTP-Version CRLF
 If request has body, Content-Length must be available

    Full-Response  = Status-Line              ; Section 6.1
                     *( General-Header        ; Section 4.3
                      | Response-Header       ; Section 6.2
                      | Entity-Header )       ; Section 7.1
                      CRLF
                     [ Entity-Body ]          ; Section 7.2

    Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF

    Simple-Request  = "GET" SP Request-URI CRLF

    Simple-Response = [ Entity-Body ]

  HTTP/1.1

  transfer-coding         = "chunked" | transfer-extension
       transfer-extension      = token *( ";" parameter )

   Parameters are in  the form of attribute/value pairs.

       parameter               = attribute "=" value
       attribute               = token
       value                   = token | quoted-string

 Chunked-Body   = *chunk
                        last-chunk
                        trailer
                        CRLF

       chunk          = chunk-size [ chunk-extension ] CRLF
                        chunk-data CRLF
       chunk-size     = 1*HEX
       last-chunk     = 1*("0") [ chunk-extension ] CRLF

       chunk-extension= *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
       chunk-ext-name = token
       chunk-ext-val  = token | quoted-string
       chunk-data     = chunk-size(OCTET)
       trailer        = *(entity-header CRLF)



 1.Any response message which "MUST NOT" include a message-body (such
     as the 1xx, 204, and 304 responses and any response to a HEAD
     request) is always terminated by the first empty line after the
     header fields, regardless of the entity-header fields present in
     the message.

 2.If a Transfer-Encoding header field (section 14.41) is present and
     has any value other than "identity", then the transfer-length is
     defined by use of the "chunked" transfer-coding (section 3.6),
     unless the message is terminated by closing the connection.

 3.If a Content-Length header field (section 14.13) is present, its
     decimal value in OCTETs represents both the entity-length and the
     transfer-length. The Content-Length header field MUST NOT be sent
     if these two lengths are different (i.e., if a Transfer-Encoding
     header field is present). If a message is received with both a
     Transfer-Encoding header field and a Content-Length header field,
     the latter MUST be ignored.

If either the client or the server sends the close token in the
   Connection header, that request becomes the last one for the
   connection.

       Connection = "Connection" ":" 1#(connection-token)
       connection-token  = token

  Host = "Host" ":" host [ ":" port ]

  Remove trailers
  TE        = "TE" ":" #( t-codings )
  t-codings = "trailers" | ( transfer-extension [ accept-params ] )

Range header with ultiple byte-
     range specifiers from a 1.1 client implies that the lient can parse
     multipart/byteranges responses.

 Transfer-Encoding       = "Transfer-Encoding" ":" 1#transfer-coding


*/

static bool proxy_flush(http_proxy_t proxy, bool force);

http_proxy_t http_proxy_new(const char* sourcehost, const char* targethost,
                            buf_t output)
{
    http_proxy_t proxy = calloc(1, sizeof(struct _http_proxy_t));

    assert(sourcehost && targethost);
    assert(*sourcehost || !*targethost);
    assert(*targethost || !*sourcehost);

    proxy->sourcehost = strdup(sourcehost);
    proxy->targethost = strdup(targethost);
    proxy->output = output;

    proxy->input = buf_new(1024);

    return proxy;
}

void* http_proxy_wptr(http_proxy_t proxy, size_t* avail)
{
    return buf_wptr(proxy->input, avail);
}

size_t http_proxy_wmove(http_proxy_t proxy, size_t amount)
{
    size_t avail = buf_wmove(proxy->input, amount);
    if (proxy_flush(proxy, false))
    {
        avail = buf_wavail(proxy->input);
    }
    return avail;
}

size_t http_proxy_write(http_proxy_t proxy, const void* data, size_t max)
{
    size_t ret = buf_write(proxy->input, data, max);
    proxy_flush(proxy, false);
    return ret;
}

bool http_proxy_flush(http_proxy_t proxy)
{
    proxy_flush(proxy, true);
    return buf_ravail(proxy->input) == 0;
}

void http_proxy_free(http_proxy_t proxy)
{
    if (proxy == NULL)
    {
        return;
    }

    free(proxy->sourcehost);
    free(proxy->targethost);
    free(proxy->tmpstr);

    buf_free(proxy->input);
    free(proxy);
}

static inline size_t iter_pos(iter_t i)
{
    return i.pos - i.ptr;
}

static inline char iter_get(iter_t i, ssize_t off)
{
    assert(i.pos < i.end);
    return i.pos[off];
}

static inline void iter_move(iter_t* iter, ssize_t off)
{
    if (off < 0)
    {
        assert(iter->pos + off >= iter->ptr);
    }
    else if (off > 0)
    {
        assert(iter->pos + off <= iter->end);
    }

    iter->pos += off;
}

static int iter_cmp(iter_t i, iter_t j)
{
    assert(i.ptr == j.ptr);
    if (i.pos == j.pos)
    {
        return 0;
    }
    return i.pos - j.pos;
}

static void iter_begin(http_proxy_t proxy, iter_t* iter)
{
    size_t avail;
    iter->ptr = buf_rptr(proxy->input, &avail);
    iter->end = iter->ptr + avail;
    iter->pos = iter->ptr;
}

/*
static void iter_end(http_proxy_t proxy, iter_t* iter)
{
    size_t avail;
    iter->ptr = buf_rptr(proxy->input, &avail);
    iter->end = iter->ptr + avail;
    iter->pos = iter->end;
}
*/

static inline void iter_copy(iter_t* dst, iter_t src)
{
    *dst = src;
}

static inline bool issp(char c)
{
    return (c == ' ' || c == '\t');
}

static bool find_newline(iter_t offset, iter_t* iter, bool allow_lws,
                         bool allow_quoted)
{
    const char* quote_start = NULL;
    iter_copy(iter, offset);
    for (; iter->pos < iter->end; ++(iter->pos))
    {
        if (allow_quoted)
        {
            if (quote_start == NULL)
            {
                if (*(iter->pos) == '"')
                {
                    quote_start = iter->pos;
                    continue;
                }
            }
            else
            {
                if (*(iter->pos) == '\\')
                {
                    ++(iter->pos);
                }
                else if (*(iter->pos) == '"')
                {
                    quote_start = NULL;
                }
                continue;
            }
        }
        if (*(iter->pos) == '\n')
        {
            if (allow_lws && offset.pos > offset.ptr)
            {
                if (iter->pos + 1 == iter->end)
                {
                    return false;
                }
                else if (!issp(iter->pos[1]))
                {
                    return true;
                }
            }
            else
            {
                return true;
            }
        }
    }
    if (allow_quoted && quote_start != NULL)
    {
        /* As we can't save "in quoted" state, reset the iterator to the start
         * of the quoted area */
        iter->pos = quote_start;
    }
    return false;
}

static bool find_char(iter_t offset, char c, iter_t* iter, bool allow_quoted)
{
    const char* quote_start = NULL;
    iter_copy(iter, offset);
    for (; iter->pos < iter->end; ++(iter->pos))
    {
        if (allow_quoted)
        {
            if (quote_start == NULL)
            {
                if (*(iter->pos) == '"')
                {
                    quote_start = iter->pos;
                    continue;
                }
            }
            else
            {
                if (*(iter->pos) == '\\')
                {
                    ++(iter->pos);
                }
                else if (*(iter->pos) == '"')
                {
                    quote_start = NULL;
                }
                continue;
            }
        }
        if (*(iter->pos) == c)
        {
            return true;
        }
    }
    if (allow_quoted && quote_start != NULL)
    {
        /* As we can't save "in quoted" state, reset the iterator to the start
         * of the quoted area */
        iter->pos = quote_start;
    }
    return false;
}

static bool find_chars(iter_t offset, const char* cs, iter_t* iter,
                       bool allow_quoted)
{
    const char* quote_start = NULL;
    iter_copy(iter, offset);
    for (; iter->pos < iter->end; ++(iter->pos))
    {
        if (allow_quoted)
        {
            if (quote_start == NULL)
            {
                if (*(iter->pos) == '"')
                {
                    quote_start = iter->pos;
                    continue;
                }
            }
            else
            {
                if (*(iter->pos) == '\\')
                {
                    ++(iter->pos);
                }
                else if (*(iter->pos) == '"')
                {
                    quote_start = NULL;
                }
                continue;
            }
        }
        if (strchr(cs, *(iter->pos)) != NULL)
        {
            return true;
        }
    }
    if (allow_quoted && quote_start != NULL)
    {
        /* As we can't save "in quoted" state, reset the iterator to the start
         * of the quoted area */
        iter->pos = quote_start;
    }
    return false;
}

static void eat_sp(iter_t* iter)
{
    while (iter->pos < iter->end && issp(*(iter->pos)))
    {
        ++(iter->pos);
    }
}

static void eat_crlf(iter_t* iter)
{
    if (iter->pos < iter->end && *(iter->pos) == '\r')
    {
        ++(iter->pos);
    }
    if (iter->pos < iter->end && *(iter->pos) == '\n')
    {
        ++(iter->pos);
    }
}

static void remove_lws(char* str, size_t len, bool ends_in_sp)
{
    char* pos, *last = str;
    for (;;)
    {
        pos = strchr(last, '\n');
        if (pos == NULL)
        {
            return;
        }
        if ((ends_in_sp && pos[1] == '\0') || issp(pos[1]))
        {
            if (pos > last && pos[-1] == '\r')
            {
                memmove(pos - 1, pos + 1, (len - ((pos + 1) - str)) + 1);
                len -= 2;
                last = pos;
            }
            else
            {
                memmove(pos, pos + 1, (len - ((pos + 1) - str)) + 1);
                len--;
                last = pos + 1;
            }
            if (last >= (str + len))
            {
                return;
            }
        }
        else
        {
            last = pos + 1;
        }
    }
}

static bool alloc_str(http_proxy_t proxy, size_t need)
{
    if (need >= proxy->tmplen)
    {
        char* tmp;
        size_t ns = proxy->tmplen * 2;
        if (ns <= need)
            ns = need + 1;
        if (ns < 256)
            ns = 256;
        tmp = realloc(proxy->tmpstr, ns);
        if (tmp == NULL)
        {
            ns = need + 1;
            tmp = realloc(proxy->tmpstr, ns);
            if (tmp == NULL)
            {
                assert(false);
                return false;
            }
        }
        proxy->tmpstr = tmp;
        proxy->tmplen = ns;
    }
    return true;
}

static char* read_str(http_proxy_t proxy, iter_t start, iter_t end,
                      bool ends_in_sp)
{
    size_t need;
    assert(start.ptr == end.ptr);
    need = end.pos - start.pos;
    if (!alloc_str(proxy, need))
    {
        if (proxy->tmpstr != NULL)
            proxy->tmpstr[0] = '\0';
        return proxy->tmpstr;
    }
    memcpy(proxy->tmpstr, start.pos, need);
    proxy->tmpstr[need] = '\0';
    remove_lws(proxy->tmpstr, need, ends_in_sp);
    return proxy->tmpstr;
}

static inline bool is_digit(char c)
{
    return (c >= '0' && c <= '9');
}

static bool parse_http_version(const char* http_version, unsigned int* major,
                               unsigned int* minor)
{
    size_t len = strlen(http_version);
    char* end;
    const char* pos;
    unsigned long tmp;
    /* HTTP-Version   = "HTTP" "/" 1*DIGIT "." 1*DIGIT */
    if (len < 8)
    {
        return false;
    }
    if (memcmp(http_version, "HTTP/", 5) != 0 ||
        !is_digit(http_version[5]))
    {
        return false;
    }
    errno = 0;
    tmp = strtoul(http_version + 5, &end, 10);
    if (errno || !end || *end != '.' || tmp > UINT_MAX)
    {
        return false;
    }
    *major = (unsigned int)tmp;
    pos = end + 1;
    tmp = strtoul(pos, &end, 10);
    if (errno || !end || *end != '\0' || tmp > UINT_MAX)
    {
        return false;
    }
    *minor = (unsigned int)tmp;
    return true;
}

static inline bool is_separator(char c)
{
    return strchr("()<>@,;:\\\"/[]?={} \t", c) != NULL;
}

static inline bool is_ctl(char c)
{
    return (c < ' ' || c == '\x7f');
}

static inline bool is_char(char c)
{
    return (c & 0x80) == 0;
}

static bool valid_token(const char* str)
{
    const char* pos = str;
    if (*pos == '\0')
    {
        return false;
    }
    for (; *pos != '\0'; ++pos)
    {
        if (!is_char(*pos) || is_ctl(*pos) || is_separator(*pos))
        {
            return false;
        }
    }
    return true;
}

static bool valid_metod(const char* str)
{
    return valid_token(str);
}

static inline bool is_lowalpha(char c)
{
    return (c >= 'a' && c <= 'z');
}

static inline bool is_upalpha(char c)
{
    return (c >= 'A' && c <= 'Z');
}

static inline bool is_uri_mark(char c)
{
    return strchr("-_.!~*'()", c) != NULL;
}

static inline bool is_uri_unreserved(char c)
{
    return is_digit(c) || is_lowalpha(c) || is_upalpha(c) || is_uri_mark(c);
}

static inline bool is_uri_reserved(char c)
{
    return strchr(";/?:@&=+$,", c) != NULL;
}

static inline bool is_hex(char c)
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool valid_request_uri(const char* str)
{
    const char* pos;
    if (*str == '\0')
    {
        return false;
    }
    if (strcmp(str, "*") == 0)
    {
        return true;
    }
    for (pos = str; *pos != '\0'; ++pos)
    {
        if (*pos == '%')
        {
            /* escaped */
            ++pos;
            if (!is_hex(*pos) || !is_hex(pos[1]))
            {
                return false;
            }
            ++pos;
        }
        if (!is_uri_unreserved(*pos) && !is_uri_reserved(*pos))
        {
            return false;
        }
    }
    return true;
}

static bool transfer_flush(http_proxy_t proxy)
{
    size_t avail, wrote;
    const char* ptr;
    assert(proxy->active_transfer);
    assert(buf_ravail(proxy->input) >= proxy->transfer_left);
    for (;;)
    {
        ptr = buf_rptr(proxy->input, &avail);
        if (avail > proxy->transfer_left)
        {
            avail = proxy->transfer_left;
        }
        wrote = buf_write(proxy->output, ptr, avail);
        buf_rmove(proxy->input, wrote);
        proxy->transfer_left -= wrote;
        if (wrote == 0 || proxy->transfer_left == 0)
        {
            break;
        }
    }

    if (proxy->transfer_left == 0)
    {
        proxy->active_transfer = false;
        iter_begin(proxy, &proxy->last);
        return true;
    }
    return false;
}

/* Note that this method makes all iterators invalid except proxy->last which
 * it moves to the start of the remaining data */
static void transfer(http_proxy_t proxy, iter_t end)
{
    assert(!proxy->active_transfer && !proxy->active_replace);
    if (end.pos > end.ptr)
    {
        proxy->active_transfer = true;
        proxy->transfer_left = end.pos - end.ptr;
        transfer_flush(proxy);
    }
    else
    {
        iter_begin(proxy, &proxy->last);
    }
}

/* Note that this method makes all iterators invalid except proxy->last which
 * it moves to the start of the remaining data */
static void ignore(http_proxy_t proxy, iter_t end)
{
    assert(!proxy->active_transfer && !proxy->active_replace);
    if (end.pos > end.ptr)
    {
        buf_rmove(proxy->input, end.pos - end.ptr);
    }
    iter_begin(proxy, &proxy->last);
}

static bool replace_flush(http_proxy_t proxy)
{
    size_t avail, wrote;
    assert(proxy->active_replace && !proxy->active_transfer);
    if (proxy->replace_skip > 0)
    {
        wrote = buf_skip(proxy->input, proxy->replace_skip);
        proxy->replace_skip -= wrote;
        if (proxy->replace_skip > 0)
        {
            return true;
        }
    }
    for (;;)
    {
        avail = strlen(proxy->replace);
        wrote = buf_write(proxy->output, proxy->replace, avail);
        proxy->replace += wrote;
        if (wrote == 0 || *(proxy->replace) == '\0')
        {
            break;
        }
    }

    if (*(proxy->replace) == '\0')
    {
        proxy->active_replace = false;
        proxy->replace = NULL;
        iter_begin(proxy, &proxy->last);
        return true;
    }
    return false;
}

/* Note that this method makes all iterators invalid except proxy->last which
 * it moves to the start of the remaining data */
static void replace(http_proxy_t proxy, iter_t start, const char *content,
                    iter_t end)
{
    size_t skip = end.pos - start.pos;
    assert(start.ptr == end.ptr);
    assert(!proxy->active_transfer && !proxy->active_replace);
    transfer(proxy, start);
    if (skip > 0 || *content != '\0')
    {
        proxy->active_replace = true;
        proxy->replace_skip = skip;
        proxy->replace = content;
        if (!proxy->active_transfer)
        {
            replace_flush(proxy);
        }
    }
}

static void reset_state(http_proxy_t proxy)
{
    proxy->request = false;
    proxy->response_code = 0;
    proxy->major = proxy->minor = 0;
    proxy->content_pos = 0;
    proxy->content_length_set = false;
    proxy->content_length = 0;
    proxy->chunked = false;
    proxy->closed = false;
    proxy->in_chunk = false;
}

static bool dawn(http_proxy_t proxy, bool force)
{
    iter_t end, start, pos;
    const char* str;
    /* Request-Line = Method SP Request-URI SP HTTP-Version CRLF */
    /* Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF */
    /* Simple-Request = "GET" SP Request-URI CRLF */
    /* Simple-Response = [ Entity-Body ] */
    if (!find_newline(proxy->last, &proxy->last, false, false))
    {
        if (force)
        {
            if (proxy->last.end == proxy->last.ptr)
            {
                return false;
            }
            goto simple_response;
        }
        return false;
    }
    iter_copy(&end, proxy->last);
    if (iter_pos(end) > 0 && iter_get(end, -1) == '\r')
    {
        iter_move(&end, -1);
    }
    iter_begin(proxy, &start);
    if (iter_cmp(start, end) == 0)
    {
        /* Ignore empty lines in start of response/request */
        eat_crlf(&end);
        ignore(proxy, end);
        return dawn(proxy, force);
    }
    if (!find_chars(start, " \t", &pos, false) || iter_cmp(pos, end) > 0)
    {
        goto simple_response;
    }
    str = read_str(proxy, start, pos, true);
    if (*str == '\0')
    {
        goto simple_response;
    }
    if (strncmp(str, "HTTP/", 5) == 0)
    {
        /* Status-Line ? */
        if (!parse_http_version(str, &(proxy->major), &(proxy->minor)))
        {
            goto simple_response;
        }
        eat_sp(&pos);
        iter_copy(&start, pos);
        if (!find_chars(start, " \t", &pos, false) || iter_cmp(pos, end) > 0)
        {
            goto simple_response;
        }
        str = read_str(proxy, start, pos, true);
        if (strlen(str) != 3 ||
            !is_digit(str[0]) || !is_digit(str[1]) || !is_digit(str[2]))
        {
            goto simple_response;
        }
        proxy->response_code = (str[0] - '0') * 100
            + (str[1] - '0') * 10 + (str[2] - '0');
        /* HTTP version & valid status code, the rest is response phrase */
        eat_crlf(&end);
        proxy->request = false;
        proxy->state = STATE_HEADER;
        transfer(proxy, end);
        return true;
    }
    if (strcasecmp(str, "GET") == 0)
    {
        /* Simple-Request ? */
        eat_sp(&pos);
        iter_copy(&start, pos);
        if (!find_chars(start, " \t", &pos, false) || iter_cmp(pos, end) > 0)
        {
            /* Simple-Request ? */
            str = read_str(proxy, start, end, true);
            if (!valid_request_uri(str))
            {
                goto simple_response;
            }
            /* Simple-Request it is - not really a body here but using it as a
             * "just push the rest through" */
            eat_crlf(&end);
            proxy->request = true;
            proxy->major = 0;
            proxy->minor = 9;
            proxy->state = STATE_BODY;
            transfer(proxy, end);
            return true;
        }
    }
    else
    {
        /* Request-Line ? */
        if (!valid_metod(str))
        {
            goto simple_response;
        }
        eat_sp(&pos);
        iter_copy(&start, pos);
        if (!find_chars(start, " \t", &pos, false) || iter_cmp(pos, end) > 0)
        {
            goto simple_response;
        }
    }

    /* Request-Line ? */
    str = read_str(proxy, start, pos, true);
    if (!valid_request_uri(str))
    {
        goto simple_response;
    }
    eat_sp(&pos);
    iter_copy(&start, pos);
    str = read_str(proxy, start, end, false);
    if (!parse_http_version(str, &(proxy->major), &(proxy->minor)))
    {
        goto simple_response;
    }
    /* Request-Line it is */
    eat_crlf(&end);
    proxy->request = true;
    proxy->state = STATE_HEADER;
    transfer(proxy, end);
    return true;

 simple_response:
#ifdef DEBUG
    fprintf(stderr, "http_proxy: Fallback to HTTP message as a simple-response\n");
#endif
    proxy->state = STATE_BODY;
    proxy->request = false;
    proxy->response_code = 200;
    proxy->major = 0;
    proxy->minor = 9;
    proxy->content_length_set = false;
    proxy->content_length = 0;
    proxy->closed = true;
    proxy->chunked = false;
    iter_begin(proxy, &proxy->last);
    return false;
}

static char* skip_token(char* str)
{
    char* pos = str;
    for (; *pos != '\0'; ++pos)
    {
        if (!is_char(*pos) || is_ctl(*pos) || is_separator(*pos))
        {
            break;
        }
    }
    return pos > str ? pos : NULL;
}

static char* skip_quoted_string(char* str)
{
    char* pos = str;
    if (*pos != '\"')
    {
        return NULL;
    }
    ++pos;
    for (; *pos != '\0'; ++pos)
    {
        if (*pos == '"')
        {
            return pos + 1;
        }
        else if (*pos == '\\')
        {
            ++pos;
            if ((*pos & 0x80) != 0)
            {
                /* "\" CHAR = <(octets 0 - 127)> */
                break;
            }
        }
        else if (*pos != '\r' && *pos != '\t' && *pos != '\n' && is_ctl(*pos))
        {
            /* <any OCTET except CTLs but including LWS> */
            break;
        }
    }
    return NULL;
}

static char* skip_parameter(char* str)
{
    char* pos;
    if (*str != ';')
        return str;
    pos = skip_token(str + 1);
    if (pos != NULL && *pos == '=')
    {
        if (pos[1] == '"')
        {
            pos = skip_quoted_string(pos + 1);
        }
        else
        {
            pos = skip_token(pos + 1);
        }
    }
    if (pos != NULL && (*pos == ';' || *pos == '\0'))
    {
        return pos;
    }
    else
    {
        return NULL;
    }
}

static bool parse_content_length(const char* str, uint64_t* length)
{
    uint64_t ret = 0, last;
    const char* pos = str;
    if (!is_digit(*str))
    {
        return false;
    }
    for (;;)
    {
        if (!*pos)
        {
            *length = ret;
            return true;
        }
        if (!is_digit(*pos))
        {
            return false;
        }
        last = ret;
        ret *= 10;
        if (last > 0 && (ret <= last || ret / 10 != last))
        {
            /* overflow */
            return false;
        }
        ret += (uint64_t)(*pos - '0');
        ++pos;
    }
}

static bool header_value_list_contains(char* str, const char* token)
{
    char* pos = str, *start;
    size_t tokenlen = strlen(token);
    for (;;)
    {
        for (; issp(*pos); ++pos);
        if (*pos == '\0')
        {
            return false;
        }
        if (*pos == ',')
        {
            /* null element */
            continue;
        }
        start = pos;
        pos = skip_token(start);
        if (pos == NULL)
        {
            return false;
        }
        if (tokenlen == (pos - start) &&
            strncasecmp(token, start, tokenlen) == 0)
        {
            return true;
        }
        while (*pos == ';')
        {
            pos = skip_parameter(pos);
            if (pos == NULL)
            {
                return false;
            }
        }
    }
}

static void header_value_list_remove(char* str, const char* token)
{
    char* pos = str, *start, *end;
    size_t tokenlen = strlen(token);
    for (;;)
    {
        for (; issp(*pos); ++pos);
        if (*pos == '\0')
        {
            return;
        }
        if (*pos == ',')
        {
            /* null element */
            continue;
        }
        start = pos;
        pos = skip_token(start);
        if (pos == NULL)
        {
            return;
        }
        end = pos;
        while (*pos == ';')
        {
            pos = skip_parameter(pos);
            if (pos == NULL)
            {
                return;
            }
        }
        if (tokenlen == (end - start) &&
            strncasecmp(token, start, tokenlen) == 0)
        {
            memmove(start, pos, strlen(pos) + 1);
            return;
        }
    }
}

static bool header(http_proxy_t proxy, bool force)
{
    iter_t start, end;
    char* str, *pos, *tmp;
    if (!find_newline(proxy->last, &proxy->last, true, true))
    {
        if (force)
        {
            goto invalid_header;
        }
        return false;
    }
    iter_copy(&end, proxy->last);
    if (iter_pos(end) > 0 && iter_get(end, -1) == '\r')
    {
        iter_move(&end, -1);
    }
    iter_begin(proxy, &start);
    if (iter_cmp(start, end) == 0)
    {
        /* End of headers, start of body */
        eat_crlf(&end);
        if (proxy->major < 1 ||
            (proxy->major == 1 && proxy->minor < 1))
        {
            proxy->closed = true;
        }
        proxy->state = STATE_BODY;
        transfer(proxy, end);
        return true;
    }
    str = read_str(proxy, start, end, false);
    pos = skip_token(str);
    if (pos == NULL || *pos != ':')
    {
        goto invalid_header;
    }
    tmp = pos;
    *pos = '\0';
    for (++pos; issp(*pos); ++pos);
    if (proxy->request && strcasecmp(str, "Host") == 0)
    {
        if (strcasecmp(pos, proxy->sourcehost) == 0)
        {
            size_t hostlen = strlen(proxy->targethost);
            size_t need = (pos - str) + hostlen + 2 + 1;
            if (!alloc_str(proxy, need))
            {
                goto invalid_header;
            }
            *tmp = ':';
            memcpy(pos, proxy->targethost, hostlen);
            memcpy(pos + hostlen, "\r\n", 3);
            eat_crlf(&end);
            replace(proxy, start, str, end);
            return true;
        }
#ifdef DEBUG
        else
        {
            if (*proxy->sourcehost)
            {
                fprintf(stderr, "http_proxy: warning missmatched host: `%s` != `%s`\n",
                        proxy->sourcehost, pos);
            }
        }
#endif
    }
    else if (proxy->request && proxy->major >= 1 && strcasecmp(str, "TE") == 0)
    {
        if (header_value_list_contains(pos, "trailers"))
        {
            *tmp = ':';
            header_value_list_remove(pos, "trailers");
            /* removing trailers will have left enough space for \r\n */
            strcat(pos, "\r\n");
            eat_crlf(&end);
            replace(proxy, start, str, end);
            return true;
        }
    }
    /*
    else if (proxy->request && proxy->major >= 1 &&
             strcasecmp(str, "Range") == 0)
    {
    }
    */
    /*
    else if (!proxy->request && strcasecmp(str, "Location") == 0)
    {
    }
    */
    else if (proxy->major >= 1 && strcasecmp(str, "Transfer-Encoding") == 0)
    {
        /* Skip the fact that "chunked" actually must be in the
         * transfer-encoding list and if so must be last. Atleast
         * that's how I read RFC2616 but hey, this should work */
        if (header_value_list_contains(pos, "chunked"))
        {
            proxy->chunked = true;
        }
    }
    else if (strcasecmp(str, "Content-Length") == 0)
    {
        uint64_t x;
        if (parse_content_length(pos, &x))
        {
            proxy->content_length = x;
            proxy->content_length_set = true;
        }
    }
    else if (strcasecmp(str, "Connection") == 0)
    {
        if (header_value_list_contains(pos, "close"))
        {
            proxy->closed = true;
        }
    }

    eat_crlf(&end);
    transfer(proxy, end);
    return true;

 invalid_header:
#ifdef DEBUG
    fprintf(stderr, "http_proxy: invalid_header: Fallback to HTTP message as a simple-response\n");
#endif
    proxy->state = STATE_BODY;
    proxy->request = false;
    proxy->response_code = 200;
    proxy->major = 0;
    proxy->minor = 9;
    proxy->content_length_set = false;
    proxy->content_length = 0;
    proxy->closed = true;
    proxy->chunked = false;
    iter_begin(proxy, &proxy->last);
    return false;
}

static char* skip_chunk_extension(char* str)
{
    return skip_parameter(str);
}

static bool parse_chunk_size(const char* start, const char* end, uint64_t* size)
{
    const char* pos;
    uint64_t ret = 0;
    if (start == end)
    {
        return false;
    }
    pos = start;
    for (; pos < end && *pos == '0'; ++pos);
    if (pos + 16 < end)
    {
        /* Chunk size larger than 64bit,
         * might be legal but we can't handle it */
        return false;
    }
    for (; pos < end; ++pos)
    {
        ret *= (uint64_t)16;
        if (is_digit(*pos))
        {
            ret += (uint64_t)(*pos - '0');
        }
        else if (*pos >= 'A' && *pos <= 'F')
        {
            ret += (uint64_t)((*pos - 'A') + 10);
        }
        else if (*pos >= 'a' && *pos <= 'f')
        {
            ret += (uint64_t)((*pos - 'a') + 10);
        }
        else
        {
            return false;
        }
    }
    *size = ret;
    return true;
}

static bool chunked_body(http_proxy_t proxy)
{
    if (!proxy->in_chunk)
    {
        iter_t start, end;
        char* str, *pos, *size_end;
        if (!find_newline(proxy->last, &proxy->last, true, true))
        {
            return false;
        }
        iter_copy(&end, proxy->last);
        if (iter_pos(end) > 0 && iter_get(end, -1) == '\r')
        {
            iter_move(&end, -1);
        }
        iter_begin(proxy, &start);
        str = read_str(proxy, start, end, false);
        for (pos = str; is_hex(*pos); ++pos);
        if (pos == str || (*pos != ';' && *pos != '\0'))
        {
            /* Invalid chunk header */
            goto invalid_chunk;
        }
        size_end = pos;
        while (*pos == ';')
        {
            pos = skip_chunk_extension(pos);
            if (pos == NULL)
            {
                /* Invalid chunk header */
                goto invalid_chunk;
            }
        }
        assert(*pos == '\0');
        if (!parse_chunk_size(str, size_end, &(proxy->chunk_size)))
        {
            /* Invalid chunk header */
            goto invalid_chunk;
        }
        if (proxy->chunk_size == 0)
        {
            /* Last chunk and we've disabled TE headers,
             * so no trailer possible */
            eat_crlf(&end);
            iter_copy(&start, end);
            if (!find_newline(start, &end, false, false))
            {
                /* Better luck next time */
                return false;
            }
            if (iter_pos(end) > 0 && iter_get(end, -1) == '\r')
            {
                iter_move(&end, -1);
            }
            if (iter_cmp(start, end) != 0)
            {
                /* There should really not be any trailer */
                goto invalid_chunk;
            }
            eat_crlf(&end);
            /* All chunks done with */
            proxy->state = STATE_DAWN;
            reset_state(proxy);
            transfer(proxy, end);
            return true;
        }
        eat_crlf(&end);
        proxy->in_chunk = true;
        proxy->chunk_pos = 0;
        transfer(proxy, end);
        return true;
    }
    else
    {
        uint64_t left = proxy->chunk_size - proxy->chunk_pos;
        if (left > 0)
        {
            const char* ptr;
            size_t avail, wrote;
            if (left > UINT_MAX)
            {
                left = UINT_MAX;
            }
            ptr = buf_rptr(proxy->input, &avail);
            if (avail == 0)
            {
                return false;
            }
            if (avail > left)
            {
                avail = left;
            }
            wrote = buf_write(proxy->output, ptr, avail);
            if (wrote == 0)
            {
                return false;
            }
            buf_rmove(proxy->input, wrote);
            proxy->chunk_pos += (uint64_t)wrote;
            if (proxy->chunk_pos < proxy->chunk_size)
            {
                return true;
            }

            iter_begin(proxy, &proxy->last);
        }

        {
            iter_t end;
            if (!find_newline(proxy->last, &proxy->last, false, false))
            {
                return false;
            }
            iter_copy(&end, proxy->last);
            if (iter_pos(end) > 0 && iter_get(end, -1) == '\r')
            {
                iter_move(&end, -1);
            }
            if (iter_pos(end) > 0)
            {
                /* Should not be anything between chunked data and its
                 * CRLF */
                goto invalid_chunk;
            }
            eat_crlf(&end);
            proxy->in_chunk = false;
            transfer(proxy, end);
        }

        return true;
    }

 invalid_chunk:
    /* Invalid chunk data, fallback to just copy everything */
#ifdef DEBUG
    fprintf(stderr, "http_proxy: invalid_chunk: Fallback to HTTP message non-chunked\n");
#endif
    proxy->chunked = false;
    proxy->closed = true;
    iter_begin(proxy, &proxy->last);
    return false;
}

static bool body(http_proxy_t proxy, bool force)
{
    if (!proxy->closed)
    {
        if ((!proxy->request &&
             ((proxy->response_code >= 100 && proxy->response_code < 200) ||
              proxy->response_code == 204 || proxy->response_code == 304))
            ||
            (proxy->request && !proxy->chunked && !proxy->content_length_set))
        {
            /* These messages never have a body.
             * TODO: There is one missing here, an response to a HEAD request
             * never has a body either. But we don't have access to the request
             * and response currently ... need an "actual" http proxy for that
             */
            proxy->state = STATE_DAWN;
            reset_state(proxy);
            iter_begin(proxy, &proxy->last);
            return false;
        }

        if (proxy->chunked)
        {
            if (chunked_body(proxy))
            {
                return true;
            }

            if (force)
            {
                goto forced;
            }
            return false;
        }

        if (proxy->content_length_set)
        {
            uint64_t left = proxy->content_length - proxy->content_pos;
            const char* ptr;
            size_t avail, wrote;
            if (left > UINT_MAX)
            {
                left = UINT_MAX;
            }
            ptr = buf_rptr(proxy->input, &avail);
            if (avail == 0)
            {
                return false;
            }
            if (avail > left)
            {
                avail = left;
            }
            wrote = buf_write(proxy->output, ptr, avail);
            if (wrote == 0)
            {
                return false;
            }
            buf_rmove(proxy->input, wrote);
            proxy->content_pos += (uint64_t)wrote;
            if (proxy->content_pos == proxy->content_length)
            {
                /* This message handled, now on to the next */
                proxy->state = STATE_DAWN;
                reset_state(proxy);
                iter_begin(proxy, &proxy->last);
            }
            return true;
        }
    }

 forced:
    {
        /* Message ended by being closed just transfer all available data */
        const char* ptr;
        size_t avail, wrote;
        ptr = buf_rptr(proxy->input, &avail);
        if (avail == 0)
        {
            return false;
        }
        wrote = buf_write(proxy->output, ptr, avail);
        if (wrote == 0)
        {
            return false;
        }
        buf_rmove(proxy->input, wrote);
        return true;
    }
}

bool proxy_flush(http_proxy_t proxy, bool force)
{
    bool ret = false, any = false;
    if (buf_ravail(proxy->input) == 0)
    {
        return false;
    }
    for (;;)
    {
        state_t last = proxy->state;

        if (proxy->active_transfer)
        {
            any |= buf_wavail(proxy->output);
            if (!transfer_flush(proxy))
            {
                return any;
            }
            assert(!proxy->active_transfer);
        }

        if (proxy->active_replace)
        {
            any |= buf_wavail(proxy->output);
            if (!replace_flush(proxy))
            {
                return any;
            }
            assert(!proxy->active_replace);
        }

        switch (proxy->state)
        {
        case STATE_SEASON4:
            iter_begin(proxy, &proxy->last);
            reset_state(proxy);
            proxy->state = STATE_DAWN;
            ret = false;
            break;
        case STATE_DAWN:
            ret = dawn(proxy, force);
            break;
        case STATE_HEADER:
            ret = header(proxy, force);
            break;
        case STATE_BODY:
            ret = body(proxy, force);
            break;
        }
        if (!ret && proxy->state == last)
        {
            return any;
        }
        any |= ret;
    }
}
