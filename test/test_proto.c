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

#include "daemon_proto.h"

#include <stdio.h>
#include <string.h>

#define RUN_TEST(_test) \
    ++tot; cnt += _test ? 1 : 0

static bool test1(void);

int main(int argc, char** argv)
{
    unsigned int tot = 0, cnt = 0;

    RUN_TEST(test1());

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
    case PKG_SETUP_TUNNEL:
        return "setup_tunnel";
    case PKG_CLOSE_TUNNEL:
        return "close_tunnel";
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
    pkg_create_tunnel(&pkg, 5678, 1212, host, 10026);
    if (!pkg_write(buf, &pkg))
    {
        fprintf(stderr, "test1: pkg4 did not fit\n");
        free(host);
        buf_free(buf);
        return false;
    }
    free(host);
    pkg_setup_tunnel(&pkg, 2525, true, 26100);
    if (!pkg_write(buf, &pkg))
    {
        fprintf(stderr, "test1: pkg5 did not fit\n");
        free(usn);
        buf_free(buf);
        return false;
    }
    pkg_close_tunnel(&pkg, 2424, true);
    if (!pkg_write(buf, &pkg))
    {
        fprintf(stderr, "test1: pkg6 did not fit\n");
        buf_free(buf);
        return false;
    }

    for (i = 0; i < 6; ++i)
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
                    strcmp(pkg.content.create_tunnel.host, "host") != 0 ||
                    pkg.content.create_tunnel.port != 10026)
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
            if (pkg.type == PKG_SETUP_TUNNEL)
            {
                if (pkg.content.setup_tunnel.tunnel_id != 2525 ||
                    !pkg.content.setup_tunnel.ok ||
                    pkg.content.setup_tunnel.port != 26100)
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
                fprintf(stderr, "test1:pkg%lu: expected setup_tunnel got %s\n",
                        i + 1, pkg_type_str(pkg.type));
                pkg_read(buf, &pkg);
                buf_free(buf);
                return false;
            }
            break;
        case 5:
            if (pkg.type == PKG_CLOSE_TUNNEL)
            {
                if (pkg.content.close_tunnel.tunnel_id != 2424 ||
                    !pkg.content.close_tunnel.local)
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
