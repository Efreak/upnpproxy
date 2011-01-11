#include "common.h"

#include "daemon_proto.h"

#include <stdio.h>
#include <string.h>

#define RUN_TEST(_test) \
    ++tot; cnt += _test ? 1 : 0

static bool test1(void);
static bool test2(size_t bufsize, size_t pkgcount, size_t pkgsize);
static bool test3(size_t bufsize, size_t pkgsize);

int main(int argc, char** argv)
{
    unsigned int tot = 0, cnt = 0;

    RUN_TEST(test1());
    RUN_TEST(test2(512, 1000, 64));
    RUN_TEST(test2(512, 1000, 100));
    RUN_TEST(test2(500, 1000, 137));
    RUN_TEST(test3(512, 256));
    RUN_TEST(test3(512, 512));
    RUN_TEST(test3(512, 1024));
    RUN_TEST(test3(100, 137));

    fprintf(stdout, "OK %u/%u\n", cnt, tot);

    return cnt == tot ? EXIT_SUCCESS : EXIT_FAILURE;
}

static const char* pkg_type_str(pkg_type_t type)
{
    switch (type)
    {
    case PKG_NEW_SERVICE:
        return "new_service";
    case PKG_OLD_SERVICE:
        return "old_service";
    case PKG_CREATE_TUNNEL:
        return "create_tunnel";
    case PKG_CLOSE_TUNNEL:
        return "close_tunnel";
    case PKG_DATA_TUNNEL:
        return "data_tunnel";
    }
    return "[error]";
}

bool test1(void)
{
    pkg_t pkg;
    char* usn, *location, *service, *server, *opt, *nls, *host;
    buf_t buf = buf_new(1024);
    size_t i;
    usn = strdup("usn");
    location = strdup("location");
    service = strdup("service");
    server = strdup("server");
    opt = strdup("opt");
    nls = strdup("nls");
    pkg_new_service(&pkg, 1234, usn, location, service, NULL, NULL, NULL);
    if (!pkg_write(buf, &pkg))
    {
        fprintf(stderr, "test1: pkg1 did not fit\n");
        free(usn); free(location); free(service); free(server); free(opt);
        free(nls);
        buf_free(buf);
        return false;
    }
    pkg_new_service(&pkg, 1235, usn, location, service, server, opt, nls);
    if (!pkg_write(buf, &pkg))
    {
        fprintf(stderr, "test1: pkg2 did not fit\n");
        free(usn); free(location); free(service); free(server); free(opt);
        free(nls);
        buf_free(buf);
        return false;
    }
    free(usn); free(location); free(service); free(server); free(opt);
    free(nls);
    pkg_old_service(&pkg, 6666);
    if (!pkg_write(buf, &pkg))
    {
        fprintf(stderr, "test1: pkg3 did not fit\n");
        buf_free(buf);
        return false;
    }
    host = strdup("host");
    pkg_create_tunnel(&pkg, 5678, 1212, host);
    if (!pkg_write(buf, &pkg))
    {
        fprintf(stderr, "test1: pkg4 did not fit\n");
        free(host);
        buf_free(buf);
        return false;
    }
    free(host);
    pkg_close_tunnel(&pkg, 2424);
    if (!pkg_write(buf, &pkg))
    {
        fprintf(stderr, "test1: pkg5 did not fit\n");
        buf_free(buf);
        return false;
    }
    usn = strdup("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    pkg_data_tunnel(&pkg, 2525, true, usn, strlen(usn));
    if (!pkg_write(buf, &pkg))
    {
        fprintf(stderr, "test1: pkg6 did not fit\n");
        free(usn);
        buf_free(buf);
        return false;
    }
    free(usn);
    usn = strdup("abcdefghijklmnopqrstuvwxyz");
    pkg_data_tunnel(&pkg, 9873, false, usn, strlen(usn));
    if (!pkg_write(buf, &pkg))
    {
        fprintf(stderr, "test1: pkg7 did not fit\n");
        free(usn);
        buf_free(buf);
        return false;
    }
    free(usn);

    for (i = 0; i < 7; ++i)
    {
        if (!pkg_peek(buf, &pkg))
        {
            fprintf(stderr, "test1:pkg%lu: pkg_peek returned false\n",
                    i + 1);
            buf_free(buf);
            return false;
        }
        switch (i)
        {
        case 0:
            if (pkg.type == PKG_NEW_SERVICE)
            {
                if (pkg.content.new_service.service_id != 1234 ||
                    strcmp(pkg.content.new_service.usn, "usn") != 0 ||
                    strcmp(pkg.content.new_service.location, "location") != 0 ||
                    strcmp(pkg.content.new_service.service, "service") != 0 ||
                    pkg.content.new_service.server != NULL ||
                    pkg.content.new_service.opt != NULL ||
                    pkg.content.new_service.nls != NULL)
                {
                    fprintf(stderr, "test1:pkg%lu: missmatched data\n",
                            i + 1);
                    pkg_read(buf, &pkg);
                    buf_free(buf);
                    return false;
                }
            }
            else
            {
                fprintf(stderr, "test1:pkg%lu: expected new_service got %s\n",
                        i + 1, pkg_type_str(pkg.type));
                pkg_read(buf, &pkg);
                buf_free(buf);
                return false;
            }
            break;
        case 1:
            if (pkg.type == PKG_NEW_SERVICE)
            {
                if (pkg.content.new_service.service_id != 1235 ||
                    strcmp(pkg.content.new_service.usn, "usn") != 0 ||
                    strcmp(pkg.content.new_service.location, "location") != 0 ||
                    strcmp(pkg.content.new_service.service, "service") != 0 ||
                    strcmp(pkg.content.new_service.server, "server") != 0 ||
                    strcmp(pkg.content.new_service.opt, "opt") != 0 ||
                    strcmp(pkg.content.new_service.nls, "nls") != 0)
                {
                    fprintf(stderr, "test1:pkg%lu: missmatched data\n",
                            i + 1);
                    pkg_read(buf, &pkg);
                    buf_free(buf);
                    return false;
                }
            }
            else
            {
                fprintf(stderr, "test1:pkg%lu: expected new_service got %s\n",
                        i + 1, pkg_type_str(pkg.type));
                pkg_read(buf, &pkg);
                buf_free(buf);
                return false;
            }
            break;
        case 2:
            if (pkg.type == PKG_OLD_SERVICE)
            {
                if (pkg.content.old_service.service_id != 6666)
                {
                    fprintf(stderr, "test1:pkg%lu: missmatched data\n",
                            i + 1);
                    pkg_read(buf, &pkg);
                    buf_free(buf);
                    return false;
                }
            }
            else
            {
                fprintf(stderr, "test1:pkg%lu: expected old_service got %s\n",
                        i + 1, pkg_type_str(pkg.type));
                pkg_read(buf, &pkg);
                buf_free(buf);
                return false;
            }
            break;
        case 3:
            if (pkg.type == PKG_CREATE_TUNNEL)
            {
                if (pkg.content.create_tunnel.service_id != 5678 ||
                    pkg.content.create_tunnel.tunnel_id != 1212 ||
                    strcmp(pkg.content.create_tunnel.host, "host") != 0)
                {
                    fprintf(stderr, "test1:pkg%lu: missmatched data\n",
                            i + 1);
                    pkg_read(buf, &pkg);
                    buf_free(buf);
                    return false;
                }
            }
            else
            {
                fprintf(stderr, "test1:pkg%lu: expected create_tunnel got %s\n",
                        i + 1, pkg_type_str(pkg.type));
                pkg_read(buf, &pkg);
                buf_free(buf);
                return false;
            }
            break;
        case 4:
            if (pkg.type == PKG_CLOSE_TUNNEL)
            {
                if (pkg.content.close_tunnel.tunnel_id != 2424)
                {
                    fprintf(stderr, "test1:pkg%lu: missmatched data\n",
                            i + 1);
                    pkg_read(buf, &pkg);
                    buf_free(buf);
                    return false;
                }
            }
            else
            {
                fprintf(stderr, "test1:pkg%lu: expected close_tunnel got %s\n",
                        i + 1, pkg_type_str(pkg.type));
                pkg_read(buf, &pkg);
                buf_free(buf);
                return false;
            }
            break;
        case 5:
            if (pkg.type == PKG_DATA_TUNNEL)
            {
                if (pkg.content.data_tunnel.tunnel_id != 2525 ||
                    !pkg.content.data_tunnel.local ||
                    pkg.content.data_tunnel.size != 26 ||
                    memcmp(pkg.content.data_tunnel.data,
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26) != 0)
                {
                    fprintf(stderr, "test1:pkg%lu: missmatched data\n",
                            i + 1);
                    pkg_read(buf, &pkg);
                    buf_free(buf);
                    return false;
                }
            }
            else
            {
                fprintf(stderr, "test1:pkg%lu: expected data_tunnel got %s\n",
                        i + 1, pkg_type_str(pkg.type));
                pkg_read(buf, &pkg);
                buf_free(buf);
                return false;
            }
            break;
        case 6:
            if (pkg.type == PKG_DATA_TUNNEL)
            {
                if (pkg.content.data_tunnel.tunnel_id != 9873 ||
                    pkg.content.data_tunnel.local ||
                    pkg.content.data_tunnel.size != 26 ||
                    memcmp(pkg.content.data_tunnel.data,
                           "abcdefghijklmnopqrstuvwxyz", 26) != 0)
                {
                    fprintf(stderr, "test1:pkg%lu: missmatched data\n",
                            i + 1);
                    pkg_read(buf, &pkg);
                    buf_free(buf);
                    return false;
                }
            }
            else
            {
                fprintf(stderr, "test1:pkg%lu: expected data_tunnel got %s\n",
                        i + 1, pkg_type_str(pkg.type));
                pkg_read(buf, &pkg);
                buf_free(buf);
                return false;
            }
            break;
        }
        pkg_read(buf, &pkg);
    }

    if (buf_ravail(buf) != 0)
    {
        fprintf(stderr, "test1: %lu bytes of data left in buffer\n",
                buf_ravail(buf));
        buf_free(buf);
        return false;
    }

    buf_free(buf);
    return true;
}

static bool test2_drain(buf_t buf, unsigned char* tmp,
                        size_t bufsize, size_t pkgcount, size_t pkgsize,
                        size_t i, size_t *j, size_t *j2)
{
    pkg_t in_pkg;

    if (!pkg_peek(buf, &in_pkg))
    {
        fprintf(stderr, "test2:%lu:%lu:%lu:%lu:%lu: pkg_peek returned false even tho buffer contains: %lu bytes\n",
                bufsize, pkgcount, pkgsize, i, *j, buf_ravail(buf));
        pkg_read(buf, &in_pkg);
        return false;
    }
    memset(tmp + pkgsize, *j + 1, pkgsize);
    if (in_pkg.type != PKG_DATA_TUNNEL ||
        in_pkg.content.data_tunnel.tunnel_id != *j ||
        in_pkg.content.data_tunnel.local != (*j % 2 == 0))
    {
        fprintf(stderr, "test2:%lu:%lu:%lu:%lu:%lu: returned pkg does not match type: %s (expected data_tunnel) tunnel_id: %lu (expected %lu) local: %c (expected %c)\n",
                bufsize, pkgcount, pkgsize, i, *j, pkg_type_str(in_pkg.type),
                (unsigned long)in_pkg.content.data_tunnel.tunnel_id,
                *j,
                in_pkg.content.data_tunnel.local ? 'Y' : 'N',
                *j % 2 == 0 ? 'Y' : 'N');
        pkg_read(buf, &in_pkg);
        return false;
    }
    if (in_pkg.content.data_tunnel.size > pkgsize)
    {
        fprintf(stderr, "test2:%lu:%lu:%lu:%lu:%lu: returned pkg too large %lu > %lu\n",
                bufsize, pkgcount, pkgsize, i, *j,
                (size_t)in_pkg.content.data_tunnel.size, pkgsize);
        pkg_read(buf, &in_pkg);
        return false;
    }
    if (in_pkg.content.data_tunnel.size + *j2 > pkgsize)
    {
        fprintf(stderr, "test2:%lu:%lu:%lu:%lu:%lu: returned pkg too large %lu + %lu > %lu\n",
                bufsize, pkgcount, pkgsize, i, *j, *j2,
                (size_t)in_pkg.content.data_tunnel.size, pkgsize);
        pkg_read(buf, &in_pkg);
        return false;
    }
    if (memcmp(in_pkg.content.data_tunnel.data, tmp + pkgsize + *j2,
               in_pkg.content.data_tunnel.size) != 0)
    {
        fprintf(stderr, "test2:%lu:%lu:%lu:%lu:%lu: returned pkg contains bad data %lu + %lu\n",
                bufsize, pkgcount, pkgsize, i, *j, *j2,
                (size_t)in_pkg.content.data_tunnel.size);
        pkg_read(buf, &in_pkg);
        return false;
    }
    *j2 += in_pkg.content.data_tunnel.size;
    if (*j2 == pkgsize)
    {
        ++*j;
        *j2 = 0;
    }
    pkg_read(buf, &in_pkg);
    return true;
}

bool test2(size_t bufsize, size_t pkgcount, size_t pkgsize)
{
    unsigned char* tmp;
    size_t i, j, j2;
    buf_t buf = buf_new(bufsize);
    tmp = malloc(pkgsize * 2);
    for (i = 0, j = 0, j2 = 0; i < pkgcount; ++i)
    {
        pkg_t out_pkg;
        memset(tmp, i + 1, pkgsize);
        pkg_data_tunnel(&out_pkg, i, i % 2 == 0, tmp, pkgsize);
        while (!pkg_write(buf, &out_pkg))
        {
            if (!test2_drain(buf, tmp, bufsize, pkgcount, pkgsize, i, &j, &j2))
            {
                buf_free(buf);
                free(tmp);
                return false;
            }
        }
    }

    while (j < pkgcount)
    {
        if (!test2_drain(buf, tmp, bufsize, pkgcount, pkgsize, i, &j, &j2))
        {
            buf_free(buf);
            free(tmp);
            return false;
        }
    }

    free(tmp);

    if (buf_ravail(buf) != 0)
    {
        fprintf(stderr, "test2:%lu:%lu:%lu: %lu bytes available in buffer at end\n", bufsize, pkgcount, pkgsize, buf_ravail(buf));
        buf_free(buf);
        return false;
    }

    buf_free(buf);
    return true;
}

bool test3(size_t bufsize, size_t pkgsize)
{
    buf_t in_buf, out_buf;
    pkg_t pkg;
    unsigned char* tmp;
    size_t i;
    tmp = malloc(pkgsize + bufsize);
    for (i = 0; i < pkgsize; ++i)
    {
        tmp[i] = (i & 0xff);
    }
    in_buf = buf_new(pkgsize * 2);
    out_buf = buf_new(bufsize);
    pkg_data_tunnel(&pkg, 1234, false, tmp, pkgsize);
    if (!pkg_write(in_buf, &pkg))
    {
        fprintf(stderr, "test3:%lu:%lu: Failed to write a pkg sized %lu into a buffer of size %lu\n",
                bufsize, pkgsize, pkgsize, pkgsize * 2);
        free(tmp);
        buf_free(in_buf);
        buf_free(out_buf);
        return false;
    }
    for (i = 0; i < pkgsize; )
    {
        while (!pkg_peek(out_buf, &pkg))
        {
            size_t avail = buf_wavail(out_buf), got, pos;
            if (avail == 0)
            {
                fprintf(stderr, "test3:%lu:%lu: pkg_peek failed but out_buf is full\n",
                        bufsize, pkgsize);
                free(tmp);
                buf_free(in_buf);
                buf_free(out_buf);
                return false;
            }
            got = buf_read(in_buf, tmp + pkgsize,
                           avail < bufsize ? avail : bufsize);
            if (got == 0)
            {
                fprintf(stderr, "test3:%lu:%lu: pkg_peek failed and in_buf only got %lu bytes\n",
                        bufsize, pkgsize, buf_ravail(in_buf));
                free(tmp);
                buf_free(in_buf);
                buf_free(out_buf);
                return false;
            }
            pos = 0;
            while (pos < got)
            {
                size_t wrote;
                wrote = buf_write(out_buf, tmp + pkgsize + pos, got - pos);
                if (wrote == 0)
                {
                    fprintf(stderr, "test3:%lu:%lu: buf_wavail seems to have lied\n",
                            bufsize, pkgsize);
                    free(tmp);
                    buf_free(in_buf);
                    buf_free(out_buf);
                    return false;
                }
                pos += wrote;
            }
        }

        if (pkg.type != PKG_DATA_TUNNEL ||
            pkg.content.data_tunnel.tunnel_id != 1234 ||
            pkg.content.data_tunnel.local ||
            pkg.content.data_tunnel.size == 0)
        {
            fprintf(stderr, "test3:%lu:%lu: corrupt pkg at %lu\n",
                    bufsize, pkgsize, i);
            pkg_read(out_buf, &pkg);
            free(tmp);
            buf_free(in_buf);
            buf_free(out_buf);
            return false;
        }

        if (i + pkg.content.data_tunnel.size > pkgsize)
        {
            fprintf(stderr, "test3:%lu:%lu: pkg too large %lu + %lu > %lu\n",
                    bufsize, pkgsize, i, (size_t)pkg.content.data_tunnel.size,
                    pkgsize);
            pkg_read(out_buf, &pkg);
            free(tmp);
            buf_free(in_buf);
            buf_free(out_buf);
            return false;
        }

        if (memcmp(tmp + i, pkg.content.data_tunnel.data,
                   pkg.content.data_tunnel.size) != 0)
        {
            fprintf(stderr, "test3:%lu:%lu: corrupt pkg at %lu\n",
                    bufsize, pkgsize, i);
            pkg_read(out_buf, &pkg);
            free(tmp);
            buf_free(in_buf);
            buf_free(out_buf);
            return false;
        }

        i += pkg.content.data_tunnel.size;
        pkg_read(out_buf, &pkg);
    }

    free(tmp);

    if (buf_ravail(out_buf) != 0)
    {
        fprintf(stderr, "test3:%lu:%lu: out_buf still has %lu bytes\n",
                bufsize, pkgsize, buf_ravail(out_buf));
        buf_free(in_buf);
        buf_free(out_buf);
        return false;
    }
    if (buf_ravail(in_buf) != 0)
    {
        fprintf(stderr, "test3:%lu:%lu: in_buf still has %lu bytes\n",
                bufsize, pkgsize, buf_ravail(in_buf));
        buf_free(in_buf);
        buf_free(out_buf);
        return false;
    }

    buf_free(in_buf);
    buf_free(out_buf);
    return true;
}
