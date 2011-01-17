#include "common.h"

#include "ssdp.h"
#include "socket.h"
#include "cfg.h"
#include "log.h"
#include "selector.h"
#include "buf.h"
#include "util.h"
#include "daemon_proto.h"
#include "map.h"
#include "vector.h"
#include "timers.h"
#include "http_proxy.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <uuid/uuid.h>

static const uint16_t DEFAULT_PORT = 24232;
static const uint16_t DEFAULT_FIRST_TUNNEL_PORT = 24235;
static const uint16_t DEFAULT_LAST_TUNNEL_PORT = 24240;
static const time_t REMOTE_EXPIRE_BUFFER = 10; /* send keep-alive 10 seconds
                                                * before the service expires */
static const time_t REMOTE_EXPIRE_TTL = 9000;

static const size_t SERVER_BUFFER_IN = 1024;
static const size_t SERVER_BUFFER_OUT = 1024;
static const size_t TUNNEL_BUFFER_LOCAL = 8192;
static const size_t TUNNEL_BUFFER_DAEMON = 8192;

/* Every 30 sec */
static const unsigned long SERVER_RECONNECT_TIMER = 30 * 1000;

typedef struct _daemon_t* daemon_t;

typedef enum _conn_state_t
{
    CONN_DEAD = 0,
    CONN_CONNECTING,
    CONN_CONNECTED,
} conn_state_t;

typedef struct _server_t
{
    daemon_t daemon;
    struct sockaddr* host;
    socklen_t hostlen;

    timecb_t reconnect_timecb;

    conn_state_t state;
    socket_t sock;

    buf_t in, out;

    /* Local as in created by this server - at me */
    map_t local_tunnels;

    /* Remote as in created by me - at this server */
    uint32_t remote_tunnel_id;
    map_t remote_tunnels;

    vector_t waiting_pkgs;
} server_t;

typedef struct _localservice_t
{
    uint32_t id;
    struct sockaddr* host;
    socklen_t hostlen;
    char* usn;
    char* location;
    char* server;
    char* service;
    char* opt;
    char* nls;
    char* service_version_pos;
    char* usn_version_pos;
    unsigned int version_max;
    time_t expires;
    timecb_t expirecb;
    daemon_t daemon;
} localservice_t;

typedef struct _remoteservice_t
{
    uint32_t source_id;
    server_t* source;
    ssdp_notify_t notify;
    char* nt_version_pos;
    char* usn_version_pos;
    unsigned int version_max;
    char* host;
    socket_t sock;
    timecb_t touchcb;
} remoteservice_t;

typedef struct _conn_t
{
    buf_t buf;
    socket_t sock;
    conn_state_t state;
} conn_t;

typedef struct _tunnel_t
{
    uint32_t id;
    /* Local conn is the connection on the daemon side to either a client
     * or service depending on "remote".
     * Daemon conn is the connection to the other daemon. */
    conn_t local_conn, daemon_conn;
    /* Remote == true if remoteservice is the source, ie the tunnel is created
     * at this daemon. */
    bool remote;
    bool stasis;
    http_proxy_t proxy;
    union {
        struct
        {
            localservice_t* service;
            server_t* server;
            char* remote_host;
            char* local_host;
        } local;
        remoteservice_t* remote;
    } source;
} tunnel_t;

typedef struct _tunnel_port_t
{
    socket_t sock;
    tunnel_t* tunnel;
    server_t* server;
    daemon_t daemon;
} tunnel_port_t;

struct _daemon_t
{
    char* cfgfile;

    bool daemonize, debug;

    log_t log;

    selector_t selector;
    timers_t timers;

    ssdp_t ssdp;

    char* bind_multicast;
    char* bind_server;
    char* bind_services;
    char* bind_tunnelport;
    uint16_t server_port;
    socket_t serv_sock;

    server_t* server;
    size_t servers;

    uint32_t local_id;
    map_t locals;
    map_t remotes;

    char* ssdp_s;
    uuid_t uuid;

    uint16_t tunnel_port_first;
    size_t tunnel_port_count;
    tunnel_port_t* tunnel_port;
};

static bool handle_args(daemon_t daemon, int argc, char** argv, int* exitcode);
static bool load_config(daemon_t daemon);
static int run_daemon(daemon_t daemon);
static void free_daemon(daemon_t daemon);

static void server_init(daemon_t daemon,
                        server_t* srv,
                        struct sockaddr* host, socklen_t hostlen);
static void server_free(daemon_t daemon, server_t* srv);
static void server_free2(server_t* srv);

static uint32_t localservice_hash(const void* _local);
static bool localservice_eq(const void* _l1, const void* _l2);
static void localservice_free(void* _local);

static uint32_t remoteservice_hash(const void* _remote);
static bool remoteservice_eq(const void* _r1, const void* _r2);
static void remoteservice_free(void* _remote);

static bool daemon_setup_remote_server(daemon_t daemon, server_t* srv);

static void daemon_server_flush_output(server_t* server);
static void daemon_server_write_pkg(server_t* server, pkg_t* pkg, bool flush);

static void daemon_tunnel_flush(tunnel_t* tunnel);

static bool parse_location(const char* location, char** proto,
                           struct sockaddr** host, socklen_t* hostlen,
                           char** path);
static char* build_location(const char* proto, const struct sockaddr* host,
                            socklen_t hostlen, const char* path);

static long daemon_remoteservice_touch(void* userdata);
static long daemon_localservice_expire(void* userdata);

int main(int argc, char** argv)
{
    struct _daemon_t daemon;
    int exitcode;
    memset(&daemon, 0, sizeof(daemon));
    daemon.serv_sock = -1;
    daemon.log = log_open();
    uuid_clear(daemon.uuid);

    if (!handle_args(&daemon, argc, argv, &exitcode))
    {
        return exitcode;
    }

    if (!load_config(&daemon))
    {
        return EXIT_FAILURE;
    }

    if (daemon.daemonize)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            chdir("/");
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            exitcode = run_daemon(&daemon);
            free_daemon(&daemon);
            return exitcode;
        }
        else if (pid < 0)
        {
            fprintf(stderr, "Failed to fork into background: %s\n",
                    strerror(errno));
            free_daemon(&daemon);
            return EXIT_FAILURE;
        }
        else
        {
            return EXIT_SUCCESS;
        }
    }
    else
    {
        exitcode = run_daemon(&daemon);
        free_daemon(&daemon);
        return exitcode;
    }
}

static void print_usage(void)
{
    fputs("Usage: `upnpproxy [OPTIONS ...]`\n", stdout);
    fputs("\n", stdout);
    fputs("Mandatory arguments to long options are mandatory for short options too.\n", stdout);
    fputs("  -C, --config=FILE    load config from FILE instead of default\n", stdout);
    fputs("  -D, --debug          run in debug mode, do not fork into background and log to stderr\n", stdout);
    fputs("  -h, --help           display this text and exit\n", stdout);
    fputs("  -V, --version        display version and exit\n", stdout);
    fputs("\n", stdout);
}

static void print_version(void)
{
    fputs("upnpproxy " VERSION " written by Joel Klinghed.\n", stdout);
}

bool handle_args(daemon_t daemon, int argc, char** argv, int* exitcode)
{
#if HAVE_GETOPT_LONG
    static const struct option long_opts[] = {
        { "help",    no_argument, NULL, 'h' },
        { "version", no_argument, NULL, 'V' },
        { "debug",   no_argument, NULL, 'D' },
        { "config",  required_argument, NULL, 'C' },
        { NULL,      0,           NULL, '\0' }
    };
#endif
    static const char* short_opts = "hVDC:";
    bool usage = false, version = false, debug = false, error = false;
    const char* cfg = NULL;
    opterr = 1;
    for (;;)
    {
        int c;
#if HAVE_GETOPT_LONG
        int idx;
        c = getopt_long(argc, argv, short_opts, long_opts, &idx);
#else
        c = getopt(argc, argv, short_opts);
#endif
        if (c == -1)
            break;

        switch (c)
        {
        case 'h':
            usage = true;
            break;
        case 'V':
            version = true;
            break;
        case 'C':
            cfg = optarg;
            break;
        case 'D':
            debug = true;
            break;
        case '?':
        default:
            error = true;
            break;
        }
    }

    if (optind < argc)
    {
        fputs("Unexpected argument after options\n", stderr);
        error = true;
    }

    if (usage)
    {
        print_usage();
        *exitcode = error ? EXIT_FAILURE : EXIT_SUCCESS;
        return false;
    }
    if (error)
    {
        fprintf(stderr, "Usage: `%s [OPTIONS ...]`\n", argv[0]);
        *exitcode = EXIT_FAILURE;
        return false;
    }
    if (version)
    {
        print_version();
        *exitcode = EXIT_SUCCESS;
        return false;
    }

    daemon->debug = debug;
    if (cfg != NULL)
    {
        daemon->cfgfile = strdup(cfg);
    }
    return true;
}

static int safestrcmp(const char* a, const char* b)
{
    if (a == NULL)
    {
        return b == NULL ? 0 : -1;
    }
    else if (b == NULL)
    {
        return 1;
    }
    return strcmp(a, b);
}

static char* safestrdup(const char* str)
{
    size_t len;
    char* ret;
    if (str == NULL)
    {
        return NULL;
    }
    len = strlen(str);
    ret = malloc(len + 1);
    memcpy(ret, str, len + 1);
    return ret;
}

static bool valid_bind(log_t log, const char* key, const char* bindaddr)
{
    struct sockaddr* addr;
    if (bindaddr == NULL)
    {
        return true;
    }
    addr = parse_addr(bindaddr, 0, NULL, true);
    if (addr == NULL)
    {
        log_printf(log, LVL_ERR,
                   "Not a valid IP address given for `%s`: `%s`",
                   key, bindaddr);
        return false;
    }
    free(addr);
    return true;
}

static bool valid_port(log_t log, const char* key, int port)
{
    if (port < 0 || port > 0xffff)
    {
        log_printf(log, LVL_ERR,
                   "Not a valid port given for `%s`: %d",
                   key, port);
        return false;
    }
    return true;
}

static bool valid_servers(daemon_t daemon, const char* key, const char* list,
                          server_t** server, size_t* servers)
{
    server_t* srv = NULL;
    size_t srvcnt = 0, alloc = 0;
    bool err = false;
    if (list != NULL)
    {
        char* tmp = strdup(list);
        char* token = strtok(tmp, " ,");
        while (token != NULL)
        {
            char* pos = strchr(token, ':');
            struct sockaddr* host;
            socklen_t hostlen;
            uint16_t port;
            if (pos == NULL)
            {
                port = DEFAULT_PORT;
            }
            else
            {
                char* end = NULL;
                long _tmp;
                *pos = '\0';
                ++pos;
                errno = 0;
                _tmp = strtol(pos, &end, 10);
                if (errno || _tmp < 0 || _tmp > 0xffff
                    || end == NULL || *end != '\0')
                {
                    log_printf(daemon->log, LVL_ERR,
                               "An invalid port found in `%s`: `%s`",
                               key, pos);
                    err = true;
                    break;
                }
                port = (uint16_t)(_tmp & 0xffff);
            }
            host = parse_addr(token, port, &hostlen, true);
            if (host == NULL)
            {
                log_printf(daemon->log, LVL_ERR,
                           "An invalid host found in `%s`: `%s`",
                           key, token);
                err = true;
                break;
            }
            if (srvcnt == alloc)
            {
                size_t na = alloc * 2;
                server_t* s;
                if (na < 4) na = 4;
                s = realloc(srv, na * sizeof(server_t));
                if (s == NULL)
                {
                    na = alloc + 10;
                    s = realloc(srv, na * sizeof(server_t));
                    if (s == NULL)
                    {
                        free(host);
                        err = true;
                        break;
                    }
                }
                srv = s;
                alloc = na;
            }
            server_init(daemon, srv + srvcnt, host, hostlen);
            ++srvcnt;
            token = strtok(NULL, " ,");
        }
        free(tmp);
    }
    if (err)
    {
        size_t s;
        for (s = 0; s < srvcnt; ++s)
        {
            server_free2(srv + s);
        }
        free(srv);
        return false;
    }

    *server = srv;
    *servers = srvcnt;
    return true;
}

static char* find_config(void)
{
    const char* dir, *home;
    char* tmp, * tmp2 = NULL;
    home = getenv("HOME");
    if (home == NULL || *home == '\0')
    {
        struct passwd* pw = getpwuid(getuid());
        if (pw != NULL)
        {
            home = pw->pw_dir;
        }
    }
    dir = getenv("XDG_CONFIG_HOME");
    if (dir == NULL || *dir == '\0')
    {
        if (home != NULL && *home != '\0')
        {
            asprintf(&tmp2, "%s/.config", home);
            dir = tmp2;
        }
    }
    if (dir != NULL && *dir != '\0')
    {
        asprintf(&tmp, "%s/upnpproxy.conf", dir);
        if (access(tmp, R_OK) == 0)
        {
            free(tmp2);
            return tmp;
        }
        free(tmp);
        free(tmp2);
        tmp2 = NULL;
    }
    dir = getenv("XDG_CONFIG_DIRS");
    if (dir == NULL)
    {
        tmp2 = strdup("/etc/xdg");
        dir = tmp2;
    }
    if (dir != NULL)
    {
        char* dirtmp = dir != tmp2 ? strdup(dir) : tmp2;
        char* token = strtok(dirtmp, ":");
        while (token != NULL)
        {
            asprintf(&tmp, "%s/upnpproxy.conf", token);
            if (access(tmp, R_OK) == 0)
            {
                free(dirtmp);
                return tmp;
            }
            free(tmp);
            token = strtok(NULL, ":");
        }
        free(dirtmp);
        tmp2 = NULL;
    }
    if (home != NULL && *home != '\0')
    {
        asprintf(&tmp, "%s/.upnpproxy.conf", dir);
        if (access(tmp, R_OK) == 0)
        {
            return tmp;
        }
        free(tmp);
    }
    return strdup(SYSCONFDIR "/upnpproxy.conf");
}

static char* find_upnp_version(char* urn, unsigned int* version)
{
    /* urn:schemas-upnp-org:service:ContentDirectory:2 */
    /* urn:schemas-upnp-org:service:ConnectionManager:2 */
    /* urn:schemas-upnp-org:device:MediaServer:2 */
    /* urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1 */
    /* uuid:*::urn:schemas-upnp-org:device:MediaServer:2 */
    char* a, * b, * c, * d;
    unsigned long tmp;
    assert(version != NULL);
    *version = 0;
    if (strncmp(urn, "urn:", 4) == 0)
    {
        a = urn + 3;
    }
    else
    {
        a = urn;
        for (;;)
        {
            b = strstr(a, "::");
            if (b == NULL)
            {
                return NULL;
            }
            if (strncmp(b, "::urn:", 6) == 0)
            {
                break;
            }
            a = b + 2;
        }
        a = b + 5;
    }

    assert(memcmp(a - 3, "urn:", 4) == 0);

    b = strchr(a + 1, ':');
    if (b == NULL)
        return NULL;
    c = strchr(b + 1, ':');
    if (c == NULL)
        return NULL;
    d = strchr(c + 1, ':');
    if (d == NULL)
        return NULL;
    ++d;
    if (*d == '\0')
        return NULL;
    errno = 0;
    tmp = strtoul(d, &a, 10);
    if (errno || !a || *a || tmp >= 1000)
    {
        return NULL;
    }
    *version = (unsigned int)tmp;
    return d;
}

static inline bool same_upnp_version(const char* search_urn, const char* search_pos, unsigned int search_version,
                                     const char* urn, const char* pos, unsigned int max_version)
{
    if (pos == NULL || search_pos == NULL)
        return false;
    if ((search_pos - search_urn) != (pos - urn))
        return false;
    if (memcmp(search_urn, urn, pos - urn) != 0)
        return false;
    return search_version <= max_version;
}

static void daemon_ssdp_search_cb(void* userdata, ssdp_search_t* search)
{
    daemon_t daemon = (daemon_t)userdata;
    size_t i;
    char* st_version_pos = NULL;
    unsigned int version;
    bool any;
    if (search->s != NULL && strcmp(search->s, daemon->ssdp_s) == 0)
    {
        /* Don't answer our own searches */
        return;
    }
    any = (strcmp(search->st, "ssdp:all") == 0);
    if (!any)
    {
        st_version_pos = find_upnp_version(search->st, &version);
    }
    for (i = map_begin(daemon->remotes); i != map_end(daemon->remotes);
         i = map_next(daemon->remotes, i))
    {
        remoteservice_t* remote = map_getat(daemon->remotes, i);
        if (any || strcmp(search->st, remote->notify.nt) == 0)
        {
            ssdp_search_response(daemon->ssdp, search, &(remote->notify));
        }
        else if (remote->nt_version_pos && st_version_pos)
        {
            if (same_upnp_version(search->st, st_version_pos, version,
                                  remote->notify.nt, remote->nt_version_pos,
                                  remote->version_max))
            {
                sprintf(remote->nt_version_pos, "%u", version);
                if (remote->usn_version_pos != NULL)
                {
                    sprintf(remote->usn_version_pos, "%u", version);
                }
                ssdp_search_response(daemon->ssdp, search, &(remote->notify));
                sprintf(remote->nt_version_pos, "%u", remote->version_max);
                if (remote->usn_version_pos != NULL)
                {
                    sprintf(remote->usn_version_pos, "%u", remote->version_max);
                }
            }
        }
    }
}

static bool daemon_add_local(daemon_t daemon, ssdp_notify_t* notify)
{
    localservice_t local, *localptr;
    time_t now = time(NULL);
    assert(notify->usn);
    if (notify->nt == NULL || notify->location == NULL ||
        notify->expires <= now)
    {
        return false;
    }
    memset(&local, 0, sizeof(localservice_t));
    if (!parse_location(notify->location, NULL,
                        &(local.host), &(local.hostlen), NULL))
    {
        log_printf(daemon->log, LVL_WARN, "Bad local service location: %s",
                   notify->location);
        return false;
    }
    local.usn = strdup(notify->usn);
    local.service = strdup(notify->nt);
    local.server = safestrdup(notify->server);
    local.opt = safestrdup(notify->opt);
    local.nls = safestrdup(notify->nls);

    local.service_version_pos = find_upnp_version(local.service,
                                                  &(local.version_max));
    if (local.service_version_pos != NULL)
    {
        unsigned int x;
        local.usn_version_pos = find_upnp_version(local.usn, &x);
        if (x != local.version_max)
        {
            local.usn_version_pos = NULL;
        }
    }

    local.location = strdup(notify->location);
    local.expires = notify->expires;
    local.daemon = daemon;
    for (;;)
    {
        local.id = ++daemon->local_id;
        if (map_get(daemon->locals, &local) == NULL)
        {
            break;
        }
    }
    localptr = map_put(daemon->locals, &local);
    localptr->expirecb = timers_add(daemon->timers,
                                    (localptr->expires - now) * 1000,
                                    localptr, daemon_localservice_expire);

    {
        /* Tell all connected servers about the new service */
        pkg_t pkg;
        size_t i;
        pkg_new_service(&pkg, localptr->id, localptr->usn, localptr->location,
                        localptr->service, localptr->server, localptr->opt,
                        localptr->nls);
        for (i = 0; i < daemon->servers; ++i)
        {
            daemon_server_write_pkg(daemon->server + i, &pkg, true);
        }
    }

    return true;
}

static void daemon_update_local(daemon_t daemon, localservice_t* local,
                                ssdp_notify_t* notify)
{
    if (notify->nts != NULL && strcmp(notify->nts, "ssdp:byebye") == 0)
    {
        map_remove(daemon->locals, local);
        return;
    }
    if (strcmp(local->service, notify->nt) != 0)
    {
        free(local->service);
        local->service = strdup(notify->nt);
        local->service_version_pos = find_upnp_version(local->service,
                                                       &(local->version_max));
    }
    if (strcmp(local->usn, notify->usn) != 0)
    {
        free(local->usn);
        local->usn = strdup(notify->usn);
        if (local->service_version_pos != NULL)
        {
            unsigned int x;
            local->usn_version_pos = find_upnp_version(local->usn, &x);
            if (x != local->version_max)
            {
                local->usn_version_pos = NULL;
            }
        }
        else
        {
            local->usn_version_pos = NULL;
        }
    }
    if (strcmp(local->location, notify->location) != 0)
    {
        struct sockaddr* host;
        socklen_t hostlen;
        if (parse_location(notify->location, NULL, &host, &hostlen, NULL))
        {
            free(local->location);
            free(local->host);
            local->location = strdup(notify->location);
            local->host = host;
            local->hostlen = hostlen;
        }
    }
    if (safestrcmp(local->server, notify->server) != 0)
    {
        free(local->server);
        local->server = safestrdup(notify->server);
    }
    if (safestrcmp(local->nls, notify->nls) != 0)
    {
        free(local->nls);
        local->nls = safestrdup(notify->nls);
    }
    if (safestrcmp(local->opt, notify->opt) != 0)
    {
        free(local->opt);
        local->opt = safestrdup(notify->opt);
    }
    if (local->expires != notify->expires)
    {
        local->expires = notify->expires;
        if (local->expirecb != NULL)
        {
            timecb_reschedule(local->expirecb,
                              (local->expires - time(NULL)) * 1000);
        }
        else
        {
            local->expirecb = timers_add(local->daemon->timers,
                                         (local->expires - time(NULL)) * 1000,
                                         local, daemon_localservice_expire);
        }
    }
}

static void daemon_ssdp_search_resp_cb(void* userdata, ssdp_search_t* search,
                                       ssdp_notify_t* notify)
{
    daemon_t daemon = (daemon_t)userdata;
    size_t i;
    bool reset_nt = false;
    char* service_pos, * usn_pos = NULL;
    unsigned int version;
    assert(search->st && notify->usn);
    if (notify->nt == NULL)
    {
        notify->nt = search->st;
        reset_nt = true;
    }
    service_pos = find_upnp_version(notify->nt, &version);
    if (service_pos != NULL)
    {
        unsigned int x;
        usn_pos = find_upnp_version(notify->usn, &x);
        if (x != version)
        {
            usn_pos = NULL;
        }
    }
    for (i = map_begin(daemon->locals); i != map_end(daemon->locals);
         i = map_next(daemon->locals, i))
    {
        localservice_t* local = map_getat(daemon->locals, i);
        if ((strcmp(local->usn, notify->usn) == 0 &&
             strcmp(local->service, notify->nt) == 0) ||
            (same_upnp_version(notify->nt, service_pos, version,
                               local->service, local->service_version_pos,
                               local->version_max)
             &&
             ((local->usn_version_pos == NULL && usn_pos == NULL) ||
              same_upnp_version(notify->usn, usn_pos, version,
                                local->usn, local->usn_version_pos,
                                local->version_max))))
        {
            daemon_update_local(daemon, local, notify);
            if (reset_nt) notify->nt = NULL;
            return;
        }
    }
    daemon_add_local(daemon, notify);
    if (reset_nt) notify->nt = NULL;
}

static void daemon_ssdp_notify_cb(void* userdata, ssdp_notify_t* notify)
{
    daemon_t daemon = (daemon_t)userdata;
    size_t i;
    char* nt_pos, * usn_pos = NULL;
    unsigned int version;
    nt_pos = find_upnp_version(notify->nt, &version);
    if (nt_pos != NULL)
    {
        unsigned int x;
        usn_pos = find_upnp_version(notify->usn, &x);
        if (x != version)
        {
            usn_pos = NULL;
        }
    }
    for (i = map_begin(daemon->remotes); i != map_end(daemon->remotes);
         i = map_next(daemon->remotes, i))
    {
        remoteservice_t* remote = map_getat(daemon->remotes, i);
        if (strcmp(remote->notify.usn, notify->usn) == 0 &&
            strcmp(remote->notify.nt, notify->nt) == 0)
        {
            /* One of our own */
            return;
        }
    }
    for (i = map_begin(daemon->locals); i != map_end(daemon->locals);
         i = map_next(daemon->locals, i))
    {
        localservice_t* local = map_getat(daemon->locals, i);
        if ((strcmp(local->usn, notify->usn) == 0 &&
             strcmp(local->service, notify->nt) == 0) ||
            (same_upnp_version(notify->nt, nt_pos, version,
                               local->service, local->service_version_pos,
                               local->version_max)
             &&
             ((local->usn_version_pos == NULL && usn_pos == NULL) ||
              same_upnp_version(notify->usn, usn_pos, version,
                                local->usn, local->usn_version_pos,
                                local->version_max))))
        {
            daemon_update_local(daemon, local, notify);
            return;
        }
    }
    if (strcmp(notify->nts, "ssdp:alive") == 0)
    {
        daemon_add_local(daemon, notify);
    }
}

static bool daemon_setup_ssdp(daemon_t daemon)
{
    ssdp_search_t search;
    assert(daemon->selector != NULL && daemon->ssdp == NULL);
    daemon->ssdp = ssdp_new(daemon->log,
                            daemon->selector,
                            daemon->timers,
                            daemon->bind_multicast,
                            daemon, daemon_ssdp_search_cb,
                            daemon_ssdp_search_resp_cb,
                            daemon_ssdp_notify_cb);
    if (daemon->ssdp == NULL)
    {
        log_puts(daemon->log, LVL_ERR, "Failed to setup SSDP");
        return false;
    }
    search.host = ssdp_getnotifyhost(daemon->ssdp, &search.hostlen);
    if (search.host != NULL)
    {
        search.s = daemon->ssdp_s;
        search.st = (char*)"ssdp:all";
        search.mx = 3;
        ssdp_search(daemon->ssdp, &search);

        free(search.host);
    }
    return true;
}

static void daemon_clear_remotes(daemon_t daemon, server_t* src)
{
    size_t i = map_begin(daemon->remotes);
    while (i != map_end(daemon->remotes))
    {
        remoteservice_t* remote = map_getat(daemon->remotes, i);
        if (remote->source == src)
        {
            i = map_removeat(daemon->remotes, i);
        }
        else
        {
            i = map_next(daemon->remotes, i);
        }
    }
}

static long reconnect_server(void* userdata)
{
    server_t* srv = userdata;
    srv->reconnect_timecb = NULL;
    daemon_setup_remote_server(srv->daemon, srv);
    return -1;
}

static void daemon_lost_server(daemon_t daemon, server_t* srv, bool wait)
{
    if (srv->sock >= 0)
    {
        selector_remove(daemon->selector, srv->sock);
        socket_close(srv->sock);
        srv->sock = -1;
    }
    if (srv->state == CONN_CONNECTED)
    {
        daemon_clear_remotes(daemon, srv);
    }
    srv->state = CONN_DEAD;
    if (wait)
    {
        if (srv->reconnect_timecb == NULL)
        {
            srv->reconnect_timecb =
                timers_add(daemon->timers, SERVER_RECONNECT_TIMER,
                           srv, reconnect_server);
        }
    }
    else
    {
        daemon_setup_remote_server(daemon, srv);
    }
}

static bool parse_location(const char* location, char** proto,
                           struct sockaddr** host, socklen_t* hostlen,
                           char** path)
{
    const char* pos, *last;
    char* tmp = NULL;
    uint16_t port;
    assert(location != NULL);
    if (proto != NULL) *proto = NULL;
    if (path != NULL) *path = NULL;
    if (host != NULL) *host = NULL;
    if (hostlen != NULL) *hostlen = 0;
    pos = strstr(location, "://");
    if (pos == NULL)
    {
        if (proto != NULL)
        {
            *proto = strdup("http");
        }
        last = location;
    }
    else
    {
        if (proto != NULL)
        {
            *proto = strndup(location, pos - location);
        }
        last = pos + 3;
    }
    pos = strchr(last, '/');
    if (pos == NULL)
    {
        tmp = strdup(last);
        last = last + strlen(last);
    }
    else
    {
        tmp = strndup(last, pos - last);
        last = pos + 1;
    }
    if (path != NULL)
    {
        *path = strdup(last);
    }
    if (tmp[0] == '[')
    {
        pos = strstr(tmp, "]:");
    }
    else
    {
        pos = strchr(tmp, ':');
    }
    if (pos == NULL)
    {
        /* TODO: Use proto to check the correct default value */
        port = 80;
    }
    else
    {
        unsigned long x;
        char* end;
        if (*pos == ']') ++pos;
        tmp[pos - tmp] = '\0';
        ++pos;
        errno = 0;
        x = strtoul(pos, &end, 10);
        if (errno || !end || *end || x < 1 || x >= 65535)
        {
            return false;
        }
        port = (uint16_t)x;
    }
    if (host != NULL)
    {
        if (tmp[0] == '[')
        {
            size_t len = strlen(tmp);
            if (tmp[len - 1] == ']')
            {
                memmove(tmp, tmp + 1, len - 2);
                tmp[len - 2] = '\0';
            }
        }
        *host = parse_addr(tmp, port, hostlen, false);
        if (*host == NULL)
        {
            return false;
        }
    }
    free(tmp);
    return true;
}

static char* build_location(const char* proto, const struct sockaddr* host,
                            socklen_t hostlen, const char* path)
{
    char* ret = NULL, * tmp = NULL, * pos;
    unsigned int port;
    assert(proto != NULL && host != NULL && path != NULL);
    assert(*proto != '\0');
    if (path[0] == '/')
    {
        ++path;
    }
    asprinthost(&tmp, host, hostlen);
    pos = strchr(tmp, ':');
    assert(pos != NULL);
    port = strtoul(pos + 1, NULL, 10);
    *pos = '\0';
    if (addr_is_ipv6(host, hostlen))
    {
        asprintf(&ret, "%s://[%s]:%u/%s", proto, tmp, port, path);
    }
    else
    {
        asprintf(&ret, "%s://%s:%u/%s", proto, tmp, port, path);
    }
    free(tmp);
    return ret;
}

static void daemon_lost_tunnel(tunnel_t* tunnel);

static void tunnel_port_read_cb(void* userdata, socket_t in_sock)
{
    tunnel_port_t* tunnel_port = userdata;
    struct sockaddr* addr;
    socklen_t addrlen;
    socket_t sock;
    tunnel_t* tunnel = tunnel_port->tunnel;
    assert(tunnel_port->sock == in_sock);

    sock = socket_accept(tunnel_port->sock, &addr, &addrlen);
    if (sock < 0)
    {
        char* tmp;
        if (socket_blockingerror(tunnel_port->sock))
        {
            return;
        }

        asprinthost(&tmp,
                    tunnel_port->server->host, tunnel_port->server->hostlen);
        log_printf(tunnel_port->daemon->log, LVL_WARN,
                   "Error accepting tunnel connection from server %s: %s",
                   tmp, socket_strerror(tunnel_port->sock));
        free(tmp);

        daemon_lost_tunnel(tunnel_port->tunnel);
        return;
    }

    if (!socket_samehost(tunnel_port->server->host,
                         tunnel_port->server->hostlen,
                         addr, addrlen))
    {
        char* srv, *host;
        asprinthost(&srv,
                    tunnel_port->server->host, tunnel_port->server->hostlen);
        asprinthost(&host, addr, addrlen);
        log_printf(tunnel_port->daemon->log, LVL_WARN,
                   "Error accepting tunnel connection from %s expected server %s",
                   host, srv);
        free(host);
        free(srv);
        free(addr);
        socket_close(sock);
        return;
    }
    free(addr);

    selector_remove(tunnel_port->daemon->selector, tunnel_port->sock);
    socket_close(tunnel_port->sock);
    tunnel_port->sock = -1;
    tunnel_port->tunnel = NULL;
    tunnel_port->server = NULL;

    if (tunnel->daemon_conn.state != CONN_CONNECTED)
    {
        if (tunnel->daemon_conn.state == CONN_CONNECTING)
        {
            selector_remove(tunnel_port->daemon->selector,
                            tunnel->daemon_conn.sock);
            socket_close(tunnel->daemon_conn.sock);
        }
        tunnel->daemon_conn.sock = sock;
        tunnel->daemon_conn.state = CONN_CONNECTED;
    }
    else
    {
        socket_close(sock);
    }
}

static uint16_t daemon_allocate_tunnel_port(daemon_t daemon, tunnel_t* tunnel,
                                            server_t* server)
{
    size_t i;
    if (daemon->tunnel_port_first == 0 || daemon->tunnel_port_count == 0)
    {
        return 0;
    }
    for (i = 0; i < daemon->tunnel_port_count; ++i)
    {
        if (daemon->tunnel_port[i].tunnel == NULL)
        {
            socket_t s = socket_tcp_listen(daemon->bind_tunnelport,
                                           daemon->tunnel_port_first + i);
            if (s < 0)
            {
                continue;
            }
            socket_setblocking(s, true);
            daemon->tunnel_port[i].daemon = daemon;
            daemon->tunnel_port[i].tunnel = tunnel;
            daemon->tunnel_port[i].server = server;
            daemon->tunnel_port[i].sock = s;
            selector_add(daemon->selector, s, daemon->tunnel_port + i,
                         tunnel_port_read_cb, NULL);
            return daemon->tunnel_port_first + i;
        }
    }
    log_printf(daemon->log, LVL_WARN, "No tunnel ports available");
    return 0;
}

static void daemon_release_tunnel_port(daemon_t daemon, tunnel_t* tunnel)
{
    size_t i;
    if (daemon->tunnel_port_first == 0)
    {
        return;
    }
    for (i = 0; i < daemon->tunnel_port_count; ++i)
    {
        if (daemon->tunnel_port[i].tunnel == tunnel)
        {
            if (daemon->tunnel_port[i].sock >= 0)
            {
                selector_remove(daemon->selector,
                                daemon->tunnel_port[i].sock);
                socket_close(daemon->tunnel_port[i].sock);
                daemon->tunnel_port[i].sock = -1;
            }
            daemon->tunnel_port[i].tunnel = NULL;
            return;
        }
    }
}

static void close_conn(daemon_t daemon, conn_t* conn)
{
    if (conn->sock >= 0)
    {
        selector_remove(daemon->selector, conn->sock);
        socket_close(conn->sock);
        conn->sock = -1;
    }
    conn->state = CONN_DEAD;
}

static void daemon_lost_tunnel(tunnel_t* tunnel)
{
    daemon_t daemon;
    if (tunnel->remote)
    {
        daemon = tunnel->source.remote->source->daemon;
        if (tunnel->daemon_conn.state >= CONN_DEAD)
        {
            pkg_t pkg;
            pkg_close_tunnel(&pkg, tunnel->id);
            daemon_server_write_pkg(tunnel->source.remote->source, &pkg, true);
        }
    }
    else
    {
        daemon = tunnel->source.local.server->daemon;
        if (tunnel->daemon_conn.state >= CONN_DEAD)
        {
            pkg_t pkg;
            pkg_close_tunnel(&pkg, tunnel->id);
            daemon_server_write_pkg(tunnel->source.local.server, &pkg, true);
        }
    }

    if (tunnel->stasis)
    {
        daemon_release_tunnel_port(daemon, tunnel);
    }

    close_conn(daemon, &tunnel->local_conn);
    close_conn(daemon, &tunnel->daemon_conn);

    if (tunnel->remote)
    {
        map_remove(tunnel->source.remote->source->remote_tunnels, tunnel);
    }
    else
    {
        map_remove(tunnel->source.local.server->local_tunnels, tunnel);
    }
}

static uint32_t local_tunnel_hash(const void* _tunnel)
{
    const tunnel_t* tunnel = _tunnel;
    assert(!tunnel->remote);
    return tunnel->id;
}

static bool local_tunnel_eq(const void* _t1, const void* _t2)
{
    const tunnel_t* t1 = _t1;
    const tunnel_t* t2 = _t2;
    assert(!t1->remote && !t2->remote);
    return t1->id == t2->id;
}

static uint32_t remote_tunnel_hash(const void* _tunnel)
{
    const tunnel_t* tunnel = _tunnel;
    assert(tunnel->remote);
    return tunnel->id;
}

static bool remote_tunnel_eq(const void* _t1, const void* _t2)
{
    const tunnel_t* t1 = _t1;
    const tunnel_t* t2 = _t2;
    assert(t1->remote && t2->remote);
    return t1->id == t2->id;
}

static void free_conn(daemon_t daemon, conn_t* conn)
{
    close_conn(daemon, conn);
    buf_free(conn->buf);
}

static void tunnel_free(tunnel_t* tunnel)
{
    daemon_t daemon;
    if (tunnel->remote)
    {
        daemon = tunnel->source.remote->source->daemon;
    }
    else
    {
        daemon = tunnel->source.local.server->daemon;
    }
    free_conn(daemon, &tunnel->local_conn);
    free_conn(daemon, &tunnel->daemon_conn);
    http_proxy_free(tunnel->proxy);
    if (!tunnel->remote)
    {
        free(tunnel->source.local.remote_host);
        free(tunnel->source.local.local_host);
    }
}

static void remote_tunnel_free(void* _tunnel)
{
    tunnel_free((tunnel_t*)_tunnel);
}

static void local_tunnel_free(void* _tunnel)
{
    tunnel_free((tunnel_t*)_tunnel);
}

static bool flush_conn(daemon_t daemon, tunnel_t* tunnel,
                       conn_t* in_conn, conn_t* out_conn,
                       http_proxy_t proxy,
                       bool* wait_read, bool* wait_write)
{
    switch (in_conn->state)
    {
    case CONN_DEAD:
        return true;
    case CONN_CONNECTING:
        *wait_read = true;
        *wait_write = true;
        return true;
    case CONN_CONNECTED:
        break;
    }

    for (;;)
    {
        size_t avail;
        ssize_t ret;
        void* ptr;
        if (proxy != NULL)
        {
            ptr = http_proxy_wptr(proxy, &avail);
        }
        else
        {
            ptr = buf_wptr(out_conn->buf, &avail);
        }
        if (avail == 0)
        {
            *wait_write = true;
            break;
        }
        ret = socket_read(in_conn->sock, ptr, avail);
        if (ret < 0)
        {
            if (socket_blockingerror(in_conn->sock))
            {
                *wait_read = true;
                break;
            }
            else
            {
                log_printf(daemon->log, LVL_WARN,
                           "%s tunnel %s connection returned error when reading: %s",
                           tunnel->remote ? "Remote" : "Local",
                           in_conn == &tunnel->local_conn ? "local" : "daemon",
                           socket_strerror(in_conn->sock));
                daemon_lost_tunnel(tunnel);
                return false;
            }
        }
        else if (ret == 0)
        {
            if (buf_ravail(in_conn->buf) > 0)
            {
                log_printf(daemon->log, LVL_WARN,
                           "%s tunnel %s connection closed before sending %lu bytes of queued data",
                           tunnel->remote ? "Remote" : "Local",
                           in_conn == &tunnel->local_conn ? "local" : "daemon",
                           buf_ravail(in_conn->buf));
            }
            if (proxy != NULL)
            {
                http_proxy_flush(proxy);
            }
            close_conn(daemon, in_conn);
            return true;
        }
        if (proxy != NULL)
        {
            if (http_proxy_wmove(proxy, ret) == 0)
            {
                break;
            }
        }
        else
        {
            if (buf_wmove(out_conn->buf, ret) == 0)
            {
                break;
            }
        }
    }

    for (;;)
    {
        ssize_t ret;
        size_t avail;
        const void* ptr = buf_rptr(in_conn->buf, &avail);
        if (avail == 0)
        {
            break;
        }
        ret = socket_write(in_conn->sock, ptr, avail);
        if (ret < 0)
        {
            if (socket_blockingerror(in_conn->sock))
            {
                *wait_write = true;
                break;
            }
            else
            {
                log_printf(daemon->log, LVL_WARN,
                           "%s tunnel %s connection returned error when writing: %s",
                           tunnel->remote ? "Remote" : "Local",
                           in_conn == &tunnel->local_conn ? "local" : "daemon",
                           socket_strerror(in_conn->sock));
                daemon_lost_tunnel(tunnel);
                return false;
            }
        }
        else if (ret == 0)
        {
            log_printf(daemon->log, LVL_WARN,
                       "%s tunnel %s connection closed when sending %lu bytes of queued data",
                       tunnel->remote ? "Remote" : "Local",
                       in_conn == &tunnel->local_conn ? "local" : "daemon",
                       avail);
            close_conn(daemon, in_conn);
            return true;
        }
        if (buf_rmove(in_conn->buf, ret) == 0)
        {
            break;
        }
    }

    return true;
}

static void daemon_tunnel_flush(tunnel_t* tunnel)
{
    daemon_t daemon;
    bool local_read = false, local_write = false;
    bool daemon_read = false, daemon_write = false;

    if (tunnel->remote)
    {
        daemon = tunnel->source.remote->source->daemon;
    }
    else
    {
        daemon = tunnel->source.local.server->daemon;
    }

    for (;;)
    {
        if (!flush_conn(daemon, tunnel,
                        &(tunnel->local_conn), &(tunnel->daemon_conn),
                        tunnel->proxy,
                        &local_read, &local_write))
        {
            return;
        }
        if (local_read && daemon_write)
        {
            break;
        }
        if (!flush_conn(daemon, tunnel,
                        &(tunnel->daemon_conn), &(tunnel->local_conn),
                        NULL,
                        &daemon_read, &daemon_write))
        {
            return;
        }
        if ((daemon_read && local_write) ||
            (tunnel->local_conn.state != CONN_CONNECTED) ||
            (tunnel->daemon_conn.state != CONN_CONNECTED))
        {
            break;
        }
    }

    if (!tunnel->stasis)
    {
        if (tunnel->remote)
        {
            if (tunnel->local_conn.state == CONN_DEAD)
            {
                daemon_lost_tunnel(tunnel);
                return;
            }
        }
        else
        {
            if (tunnel->daemon_conn.state == CONN_DEAD)
            {
                daemon_lost_tunnel(tunnel);
                return;
            }
        }
    }

    assert(daemon_read || local_read || daemon_write || local_write);

    if (tunnel->local_conn.state != CONN_DEAD)
    {
        selector_chk(daemon->selector, tunnel->local_conn.sock,
                     local_read, local_write);
    }
    if (tunnel->daemon_conn.state != CONN_DEAD)
    {
        selector_chk(daemon->selector, tunnel->daemon_conn.sock,
                     daemon_read, daemon_write);
    }
}

static void tunnel_read_cb(void* userdata, socket_t sock)
{
    tunnel_t* tunnel = userdata;
    daemon_t daemon;

    if (tunnel->remote)
    {
        daemon = tunnel->source.remote->source->daemon;
    }
    else
    {
        daemon = tunnel->source.local.server->daemon;
    }

    if (tunnel->remote)
    {
        if (tunnel->daemon_conn.sock == sock &&
            tunnel->daemon_conn.state == CONN_CONNECTING)
        {
            ssize_t ret;
            char tmp[1];
            ret = socket_read(tunnel->daemon_conn.sock, tmp, 1);
            if (!(ret < 0 && socket_blockingerror(tunnel->daemon_conn.sock)))
            {
                log_printf(daemon->log, LVL_WARN,
                           "Unable to connect tunnel to remote daemon: %s",
                           socket_strerror(tunnel->daemon_conn.sock));
                daemon_lost_tunnel(tunnel);
                return;
            }
        }
    }
    else
    {
        if (tunnel->local_conn.sock == sock &&
            tunnel->local_conn.state == CONN_CONNECTING)
        {
            ssize_t ret;
            char tmp[1];
            ret = socket_read(tunnel->local_conn.sock, tmp, 1);
            if (!(ret < 0 && socket_blockingerror(tunnel->local_conn.sock)))
            {
                log_printf(daemon->log, LVL_WARN,
                           "Unable to connect to local service: %s",
                           socket_strerror(tunnel->local_conn.sock));
                daemon_lost_tunnel(tunnel);
                return;
            }
        }
    }

    daemon_tunnel_flush(tunnel);
}

static void tunnel_write_cb(void* userdata, socket_t sock)
{
    tunnel_t* tunnel = userdata;

    if (tunnel->remote)
    {
        if (tunnel->daemon_conn.sock == sock &&
            tunnel->daemon_conn.state == CONN_CONNECTING)
        {
            tunnel->daemon_conn.state = CONN_CONNECTED;
        }
    }
    else
    {
        if (tunnel->local_conn.sock == sock &&
            tunnel->local_conn.state == CONN_CONNECTING)
        {
            tunnel->local_conn.state = CONN_CONNECTED;
        }
    }

    daemon_tunnel_flush(tunnel);
}

static void remoteservice_read_cb(void* userdata, socket_t sock)
{
    remoteservice_t *remote = userdata;
    tunnel_t tunnel, *tunnelptr;
    pkg_t pkg;
    uint16_t port;
    assert(remote->sock == sock);
    memset(&tunnel, 0, sizeof(tunnel_t));
    tunnel.local_conn.sock = socket_accept(sock, NULL, NULL);
    if (tunnel.local_conn.sock < 0)
    {
        return;
    }
    socket_setblocking(tunnel.local_conn.sock, false);
    tunnel.local_conn.state = CONN_CONNECTED;
    tunnel.remote = true;
    tunnel.source.remote = remote;
    tunnel.local_conn.buf = buf_new(TUNNEL_BUFFER_LOCAL);
    tunnel.daemon_conn.state = CONN_DEAD;
    tunnel.daemon_conn.sock = -1;
    tunnel.daemon_conn.buf = buf_new(TUNNEL_BUFFER_DAEMON);
    tunnel.proxy = http_proxy_new("", "", tunnel.daemon_conn.buf);
    for (;;)
    {
        tunnel.id = ++remote->source->remote_tunnel_id;
        if (map_get(remote->source->remote_tunnels, &tunnel) == NULL)
        {
            break;
        }
    }
    tunnelptr = map_put(remote->source->remote_tunnels, &tunnel);
    selector_add(remote->source->daemon->selector,
                 tunnelptr->local_conn.sock,
                 tunnelptr, tunnel_read_cb, tunnel_write_cb);
    selector_chkwrite(remote->source->daemon->selector,
                      tunnelptr->local_conn.sock, false);

    port = daemon_allocate_tunnel_port(remote->source->daemon, tunnelptr,
                                       remote->source);

    tunnel.stasis = true;
    pkg_create_tunnel(&pkg, remote->source_id, tunnelptr->id, remote->host,
                      port);
    daemon_server_write_pkg(remote->source, &pkg, true);
}

static void daemon_add_remote(daemon_t daemon, server_t* server,
                              pkg_new_service_t* new_service)
{
    remoteservice_t remote, *remoteptr;
    struct sockaddr* host;
    socklen_t hostlen;
    char* proto, *path;
    memset(&remote, 0, sizeof(remoteservice_t));
    remote.sock = -1;
    remote.source_id = new_service->service_id;
    remote.source = server;
    remote.notify.host = ssdp_getnotifyhost(daemon->ssdp,
                                            &(remote.notify.hostlen));
    if (remote.notify.host == NULL)
    {
        log_puts(daemon->log, LVL_ERR, "No SSDP multicast host");
        return;
    }
    remote.sock = socket_tcp_listen(daemon->bind_services, 0);
    if (remote.sock < 0 || !socket_setblocking(remote.sock, false))
    {
        log_printf(daemon->log, LVL_WARN, "Unable to listen for service: %s", socket_strerror(remote.sock));
        socket_close(remote.sock);
        free(remote.notify.host);
        return;
    }
    host = socket_getsockaddr(remote.sock, &hostlen);
    if (host == NULL)
    {
        log_puts(daemon->log, LVL_WARN, "Unable to get socket name for service socket");
        socket_close(remote.sock);
        free(remote.notify.host);
        free(host);
        return;
    }
    if (addr_is_any(host, hostlen))
    {
        /* This won't do, we need an actual address */
        uint16_t port = addr_getport(host, hostlen);
        free(host);
        host = socket_getlocalhost(remote.sock, port, &hostlen);
    }
    if (!parse_location(new_service->location, &proto, NULL, NULL, &path))
    {
        log_printf(daemon->log, LVL_WARN, "Unable to parse location: %s",
                   new_service->location);
        socket_close(remote.sock);
        free(remote.notify.host);
        free(host);
        free(proto);
        free(path);
        return;
    }
    remote.notify.location = build_location(proto, host, hostlen, path);
    if (remote.notify.location == NULL)
    {
        log_puts(daemon->log, LVL_ERR, "Unable to build location");
        socket_close(remote.sock);
        free(remote.notify.host);
        free(host);
        free(proto);
        free(path);
        return;
    }
    free(proto);
    free(path);
    asprinthost(&(remote.host), host, hostlen);
    free(host);
    if (new_service->server != NULL)
    {
        remote.notify.server = strdup(new_service->server);
    }
    if (new_service->opt != NULL)
    {
        remote.notify.opt = strdup(new_service->opt);
    }
    if (new_service->nls != NULL)
    {
        remote.notify.nls = strdup(new_service->nls);
    }
    remote.notify.usn = strdup(new_service->usn);
    remote.notify.nt = strdup(new_service->service);
    remote.notify.expires = time(NULL) + REMOTE_EXPIRE_TTL;

    remote.nt_version_pos = find_upnp_version(remote.notify.nt,
                                              &remote.version_max);
    if (remote.nt_version_pos != NULL)
    {
        unsigned int x;
        remote.usn_version_pos = find_upnp_version(remote.notify.usn,
                                                   &x);
        if (x != remote.version_max)
        {
            remote.usn_version_pos = NULL;
        }
    }

    remoteptr = map_put(daemon->remotes, &remote);
    selector_add(daemon->selector, remote.sock, remoteptr,
                 remoteservice_read_cb, NULL);
    ssdp_notify(daemon->ssdp, &(remoteptr->notify));
    remoteptr->touchcb = timers_add(daemon->timers,
                                    (REMOTE_EXPIRE_TTL - REMOTE_EXPIRE_BUFFER) * 1000,
                                    remoteptr,
                                    daemon_remoteservice_touch);
}

static void daemon_del_remote(daemon_t daemon, server_t* server,
                              pkg_old_service_t* old_service)
{
    remoteservice_t key;
    key.source_id = old_service->service_id;
    key.source = server;
    map_remove(daemon->remotes, &key);
}

static void daemon_create_tunnel(daemon_t daemon, server_t* server,
                                 pkg_create_tunnel_t* create_tunnel)
{
    tunnel_t tunnel, *tunnelptr;
    localservice_t key;
    key.id = create_tunnel->service_id;
    memset(&tunnel, 0, sizeof(tunnel_t));
    tunnel.id = create_tunnel->tunnel_id;
    tunnel.remote = false;
    tunnel.source.local.server = server;
    tunnel.source.local.service = map_get(daemon->locals, &key);
    if (tunnel.source.local.service == NULL)
    {
        pkg_t pkg;
        char* tmp;
        asprinthost(&tmp, server->host, server->hostlen);
        log_printf(daemon->log, LVL_WARN, "Server %s requesting a tunnel for non-existant service %lu",
                   tmp, key.id);
        free(tmp);
        pkg_setup_tunnel(&pkg, create_tunnel->tunnel_id, 0, false);
        daemon_server_write_pkg(server, &pkg, true);
        return;
    }
    tunnel.local_conn.sock = socket_tcp_connect2(
                                 tunnel.source.local.service->host,
                                 tunnel.source.local.service->hostlen,
                                 false);
    if (tunnel.local_conn.sock < 0)
    {
        pkg_t pkg;
        char* tmp;
        asprinthost(&tmp, tunnel.source.local.service->host,
                    tunnel.source.local.service->hostlen);
        log_printf(daemon->log, LVL_WARN, "Unable to create tunnel to %s", tmp);
        free(tmp);
        pkg_setup_tunnel(&pkg, create_tunnel->tunnel_id, 0, false);
        daemon_server_write_pkg(server, &pkg, true);
        return;
    }
    tunnel.source.local.remote_host = strdup(create_tunnel->host);
    asprinthost(&(tunnel.source.local.local_host),
                tunnel.source.local.service->host,
                tunnel.source.local.service->hostlen);
    tunnel.local_conn.state = CONN_CONNECTING;
    tunnel.local_conn.buf = buf_new(TUNNEL_BUFFER_LOCAL);
    tunnel.daemon_conn.buf = buf_new(TUNNEL_BUFFER_DAEMON);
    tunnel.proxy = http_proxy_new(tunnel.source.local.remote_host,
                                  tunnel.source.local.local_host,
                                  tunnel.daemon_conn.buf);

    tunnelptr = map_put(server->local_tunnels, &tunnel);

    selector_add(daemon->selector, tunnelptr->local_conn.sock,
                 tunnelptr, tunnel_read_cb, tunnel_write_cb);

    if (create_tunnel->port > 0)
    {
        pkg_t pkg;
        struct sockaddr* host = calloc(1, server->hostlen);
        memcpy(host, server->host, server->hostlen);
        addr_setport(host, server->hostlen, create_tunnel->port);
        tunnelptr->daemon_conn.state = CONN_CONNECTING;
        tunnelptr->daemon_conn.sock = socket_tcp_connect2(host, server->hostlen,
                                                          false);
        if (tunnelptr->daemon_conn.sock < 0)
        {
            char* tmp;
            asprinthost(&tmp, host, server->hostlen);
            log_printf(daemon->log, LVL_WARN,
                       "Unable to connect tunnel to %s", tmp);
            free(tmp);
            pkg_setup_tunnel(&pkg, create_tunnel->tunnel_id, 0, false);
            daemon_server_write_pkg(server, &pkg, true);
            free(host);
            map_remove(server->local_tunnels, tunnelptr);
            return;
        }
        free(host);

        pkg_setup_tunnel(&pkg, create_tunnel->tunnel_id, 0, true);
        daemon_server_write_pkg(server, &pkg, true);
        tunnelptr->stasis = false;

        selector_add(daemon->selector, tunnelptr->daemon_conn.sock,
                     tunnelptr, tunnel_read_cb, tunnel_write_cb);
    }
    else
    {
        uint16_t port = daemon_allocate_tunnel_port(daemon, tunnelptr, server);
        pkg_t pkg;
        if (port == 0)
        {
            log_printf(daemon->log, LVL_WARN,
                       "None of the servers had a port available");
            pkg_setup_tunnel(&pkg, create_tunnel->tunnel_id, 0, false);
            daemon_server_write_pkg(server, &pkg, true);
            map_remove(server->local_tunnels, tunnelptr);
            return;
        }

        tunnelptr->daemon_conn.state = CONN_DEAD;
        tunnelptr->daemon_conn.sock = -1;
        tunnelptr->stasis = true;

        pkg_setup_tunnel(&pkg, create_tunnel->tunnel_id, port, true);
        daemon_server_write_pkg(server, &pkg, true);
    }
}

static void daemon_close_tunnel(daemon_t daemon, server_t* server,
                                pkg_close_tunnel_t* close_tunnel)
{
    tunnel_t key;
    key.id = close_tunnel->tunnel_id;
    key.remote = false;
    key.source.local.server = server;
    map_remove(server->local_tunnels, &key);
}

#if 0
static int find_host(const char* data, size_t size, const char* host,
                     size_t* b4ptr, size_t* afptr)
{
    const char* end = data + size, *ptr;
    size_t hostlen = strlen(host);
    *b4ptr = *afptr = 0;
    for (ptr = data; ptr < end; ++ptr)
    {
        if (*ptr != '\r')
        {
            continue;
        }
        if (ptr + 1 == end)
        {
            return -1;
        }
        if (ptr[1] != '\n')
        {
            continue;
        }
        if (ptr + 4 > end)
        {
            return -1;
        }
        if (memcmp(ptr + 2, "\r\n", 2) == 0)
        {
            return 0;
        }
        if (ptr + 7 > end)
        {
            return -1;
        }
        if (memcmp(ptr + 2, "Host:", 5) != 0)
        {
            continue;
        }

        ptr += 7;

        {
            const char* value = ptr + 1, *ptr2, *end2;
            while (is_space(*value)) value++;
            for (ptr2 = value + 1; ptr2 < end; ++ptr2)
            {
                if (*ptr2 == '\n' && ptr2[-1] == '\r')
                {
                    break;
                }
            }
            if (ptr2 == end)
            {
                /* Need more data */
                return - 1;
            }
            end2 = ptr2 - 1;
            while (end2 > value && is_space(end2[-1])) --end2;
            if ((end2 - value) == hostlen &&
                memcmp(host, value, hostlen) == 0)
            {
                *b4ptr = ptr + 1 - data;
                *afptr = ptr2 - 1 - data;
                return 1;
            }
            ptr = ptr2 - 2;
        }
    }

    return 0;
}

static bool _tunnel_write_data(daemon_t daemon, tunnel_t* tunnel,
                               const char* data, size_t size)
{
    while (size > 0)
    {
        size_t wrote = buf_write(tunnel->out, data, size);
        data += wrote;
        size -= wrote;
        if (size > 0)
        {
            daemon_tunnel_flush_output(tunnel);
            if (buf_wavail(tunnel->out) == 0)
            {
                size_t s = buf_size(tunnel->out), ns;
                buf_t nbuf;
                ns = s * 2;
                if (ns < s + size)
                    ns = s + size;
                if (ns > TUNNEL_MAX_BUFFER_OUT)
                {
                    log_printf(daemon->log, LVL_WARN,
                               "Too much data for tunnel, (%lu lost)", size);
                    return false;
                }
                else
                {
                    log_printf(daemon->log, LVL_WARN,
                               "Too much data for tunnel, increasing buffer (%lu + %lu needed)", s, size);
                }
                nbuf = buf_resize(tunnel->out, ns);
                if (nbuf == NULL)
                {
                    log_printf(daemon->log, LVL_WARN,
                               "Too much data for tunnel, (%lu lost)", size);
                    return false;
                }
                tunnel->out = nbuf;
                continue;
            }
        }
    }
    return true;
}

static void daemon_data_tunnel(daemon_t daemon, server_t* server,
                               pkg_data_tunnel_t* data_tunnel)
{
    tunnel_t key, *tunnel;
    const char* in;
    size_t in_avail;

    if (data_tunnel->local)
    {
        key.id = data_tunnel->tunnel_id;
        key.remote = true;
        tunnel = map_get(server->remote_tunnels, &key);
    }
    else
    {
        key.id = data_tunnel->tunnel_id;
        key.remote = false;
        key.source.local.server = server;
        tunnel = map_get(server->local_tunnels, &key);
    }
    if (tunnel == NULL)
    {
        char* tmp;
        asprinthost(&tmp, server->host, server->hostlen);
        log_printf(daemon->log, LVL_WARN, "Got data from server %s for non-existant %s tunnel %lu", tmp, data_tunnel->local ? "remote" : "local", data_tunnel->tunnel_id);
        free(tmp);
        return;
    }
    in = data_tunnel->data;
    in_avail = data_tunnel->size;

    if (tunnel->remote)
    {
        if (!_tunnel_write_data(daemon, tunnel, in, in_avail))
        {
            daemon_lost_tunnel(tunnel);
            return;
        }
    }
    else
    {
        for (;;)
        {
            size_t b4ptr, afptr;
            size_t wrote;
            int ret = find_host(in, in_avail, tunnel->source.local.remote_host, &b4ptr, &afptr);
            if (ret < 0)
            {
                /* need more data */
                wrote = buf_write(tunnel->preout, in, in_avail);
                if (wrote < in_avail)
                {
                    log_printf(daemon->log, LVL_WARN,
                               "Too much data for tunnel (%lu lost)", in_avail);
                    daemon_lost_tunnel(tunnel);
                    return;
                }
                break;
            }
            else if (ret == 0)
            {
                /* just write whole buffer */
                if (!_tunnel_write_data(daemon, tunnel, in, in_avail))
                {
                    daemon_lost_tunnel(tunnel);
                    return;
                }
                break;
            }
            else
            {
                if (!_tunnel_write_data(daemon, tunnel, in, b4ptr))
                {
                    daemon_lost_tunnel(tunnel);
                    return;
                }
                if (!_tunnel_write_data(daemon, tunnel, tunnel->source.local.local_host, strlen(tunnel->source.local.local_host)))
                {
                    daemon_lost_tunnel(tunnel);
                    return;
                }
                in += afptr;
                in_avail -= afptr;
                if (in_avail == 0)
                {
                    break;
                }
            }
        }
    }

    daemon_tunnel_flush_output(tunnel);
}
#endif

static void daemon_setup_tunnel(daemon_t daemon, server_t* server,
                                pkg_setup_tunnel_t* setup_tunnel)
{
    tunnel_t key, *tunnel;
    key.id = setup_tunnel->tunnel_id;
    key.remote = true;
    tunnel = map_get(server->remote_tunnels, &key);
    if (tunnel == NULL)
    {
        char* tmp;
        asprinthost(&tmp, server->host, server->hostlen);
        log_printf(daemon->log, LVL_WARN, "Got setup from server %s for non-existant tunnel %lu", tmp, setup_tunnel->tunnel_id);
        free(tmp);
        return;
    }
    assert(tunnel->stasis);
    if (!setup_tunnel->ok)
    {
        char* tmp;
        asprinthost(&tmp, server->host, server->hostlen);
        log_printf(daemon->log, LVL_WARN, "Server %s failed to setup tunnel %lu", tmp, setup_tunnel->tunnel_id);
        free(tmp);
        map_remove(server->remote_tunnels, tunnel);
        return;
    }
    if (tunnel->daemon_conn.state == CONN_DEAD)
    {
        struct sockaddr* host;
        if (setup_tunnel->port == 0)
        {
            char* tmp;
            asprinthost(&tmp, server->host, server->hostlen);
            log_printf(daemon->log, LVL_WARN, "Server %s failed to provide a port for tunnel %lu", tmp, setup_tunnel->tunnel_id);
            free(tmp);
            daemon_lost_tunnel(tunnel);
            return;
        }
        host = calloc(1, server->hostlen);
        memcpy(host, server->host, server->hostlen);
        addr_setport(host, server->hostlen, setup_tunnel->port);

        tunnel->daemon_conn.state = CONN_CONNECTING;
        tunnel->daemon_conn.sock = socket_tcp_connect2(host, server->hostlen,
                                                       false);
        if (tunnel->daemon_conn.sock < 0)
        {
            char* tmp;
            asprinthost(&tmp, host, server->hostlen);
            log_printf(daemon->log, LVL_WARN,
                       "Unable to connect tunnel to %s", tmp);
            free(tmp);
            free(host);
            daemon_lost_tunnel(tunnel);
            return;
        }
        free(host);

        tunnel->stasis = false;

        selector_add(daemon->selector, tunnel->daemon_conn.sock,
                     tunnel, tunnel_read_cb, tunnel_write_cb);
    }
}

static void daemon_server_incoming_cb(void* userdata, socket_t sock)
{
    server_t* server = userdata;
    daemon_t daemon = server->daemon;
    size_t avail = 0;
    char* ptr;
    bool data_done;

    switch (server->state)
    {
    case CONN_DEAD:
        return;
    case CONN_CONNECTING:
    {
        char tmp[1];
        ssize_t got = socket_read(sock, tmp, 1);
        if (got <= 0)
        {
            char* tmp;
            if (socket_blockingerror(sock))
            {
                return;
            }

            asprinthost(&tmp, server->host, server->hostlen);
            log_printf(daemon->log, LVL_WARN,
                       "Unable to connect to server %s: %s",
                       tmp, socket_strerror(sock));
            free(tmp);
            daemon_lost_server(daemon, server, true);
            return;
        }
        else
        {
            char* tmp;
            asprinthost(&tmp, server->host, server->hostlen);
            log_printf(daemon->log, LVL_INFO,
                       "Incoming data for server %s before connection done.",
                       tmp);
            free(tmp);
            daemon_lost_server(daemon, server, true);
            return;
        }
        return;
    }
    case CONN_CONNECTED:
        break;
    }

    data_done = false;
    while (!data_done)
    {
        ptr = buf_wptr(server->in, &avail);
        if (avail > 0)
        {
            ssize_t got = socket_read(sock, ptr, avail);
            if (got < 0)
            {
                if (socket_blockingerror(sock))
                {
                    data_done = true;
                }
                else
                {
	                char* tmp;
                    asprinthost(&tmp, server->host, server->hostlen);
                    log_printf(daemon->log, LVL_WARN,
                               "Lost connection with server %s: %s",
                               tmp,
                               socket_strerror(sock));
                    free(tmp);
                    daemon_lost_server(daemon, server, false);
                    return;
                }
            }
            else if (got == 0)
            {
                char* tmp;
                asprinthost(&tmp, server->host, server->hostlen);
                log_printf(daemon->log, LVL_WARN,
                           "Lost connection with server %s: Connection closed",
                           tmp);
                free(tmp);
                daemon_lost_server(daemon, server, false);
                return;
            }
            else
            {
                buf_wmove(server->in, got);
            }
        }

        for (;;)
        {
            pkg_t pkg;
            if (pkg_peek(server->in, &pkg))
            {
                switch (pkg.type)
                {
                case PKG_NEW_SERVICE:
                    daemon_add_remote(daemon, server, &(pkg.content.new_service));
                    break;
                case PKG_OLD_SERVICE:
                    daemon_del_remote(daemon, server, &(pkg.content.old_service));
                    break;
                case PKG_CREATE_TUNNEL:
                    daemon_create_tunnel(daemon, server, &(pkg.content.create_tunnel));
                    break;
                case PKG_SETUP_TUNNEL:
                    daemon_setup_tunnel(daemon, server, &(pkg.content.setup_tunnel));
                    break;
                case PKG_CLOSE_TUNNEL:
                    daemon_close_tunnel(daemon, server, &(pkg.content.close_tunnel));
                    break;
                }
                pkg_read(server->in, &pkg);
            }
            else
            {
                assert(avail > 0);
                break;
            }
        }
    }
}

static int _daemon_server_flush_output(server_t* server);

static void daemon_server_writable_cb(void* userdata, socket_t sock)
{
    server_t* server = userdata;
    pkg_t* pkg;
    bool reflush = false;
    int flushret;

    switch (server->state)
    {
    case CONN_DEAD:
        return;
    case CONN_CONNECTING:
    {
        size_t i;
        daemon_t daemon = server->daemon;
        server->state = CONN_CONNECTED;

        for (i = map_begin(daemon->locals); i != map_end(daemon->locals);
             i = map_next(daemon->locals, i))
        {
            pkg_t pkg;
            localservice_t* local = map_getat(daemon->locals, i);
            pkg_new_service(&pkg, local->id, local->usn, local->location,
                            local->service, local->server, local->opt,
                            local->nls);
            daemon_server_write_pkg(server, &pkg, false);
        }
        break;
    }
    case CONN_CONNECTED:
        break;
    }

    flushret = _daemon_server_flush_output(server);
    if (flushret < 0)
    {
        return;
    }

    while (vector_size(server->waiting_pkgs) > 0)
    {
        size_t idx;
        reflush = true;
        for (idx = 0; idx < vector_size(server->waiting_pkgs); ++idx)
        {
            pkg = *((pkg_t**)vector_get(server->waiting_pkgs, idx));
            if (!pkg_write(server->out, pkg))
            {
                break;
            }
            else
            {
                pkg_free(pkg);
            }
        }
        vector_removerange(server->waiting_pkgs, 0, idx);
    }

    if (reflush)
    {
        flushret = _daemon_server_flush_output(server);
    }

    if (flushret == 0)
    {
        selector_chkwrite(server->daemon->selector, server->sock, false);
    }
}

static void daemon_server_accept_cb(void* userdata, socket_t sock)
{
    daemon_t daemon = (daemon_t)userdata;
    struct sockaddr* addr;
    socklen_t addrlen;
    char* tmp;
    size_t i;
    socket_t s = socket_accept(daemon->serv_sock, &addr, &addrlen);
    if (s < 0)
    {
        return;
    }
    for (i = 0; i < daemon->servers; ++i)
    {
        if (socket_samehost(daemon->server[i].host, daemon->server[i].hostlen,
                            addr, addrlen))
        {
            switch (daemon->server[i].state)
            {
            case CONN_DEAD:
                if (daemon->server[i].reconnect_timecb != NULL)
                {
                    timecb_cancel(daemon->server[i].reconnect_timecb);
                    daemon->server[i].reconnect_timecb = NULL;
                }
                daemon->server[i].state = CONN_CONNECTED;
                daemon->server[i].sock = s;
                socket_setblocking(s, false);
                selector_add(daemon->selector, daemon->server[i].sock,
                             daemon->server + i,
                             daemon_server_incoming_cb,
                             daemon_server_writable_cb);
                selector_chkwrite(daemon->selector, daemon->server[i].sock,
                                  false);
                break;
            case CONN_CONNECTING:
                selector_remove(daemon->selector, daemon->server[i].sock);
                socket_close(daemon->server[i].sock);
                daemon->server[i].state = CONN_CONNECTED;
                daemon->server[i].sock = s;
                socket_setblocking(s, false);
                selector_add(daemon->selector, daemon->server[i].sock,
                             daemon->server + i,
                             daemon_server_incoming_cb,
                             daemon_server_writable_cb);
                selector_chkwrite(daemon->selector, daemon->server[i].sock,
                                  false);
                break;
            case CONN_CONNECTED:
                socket_close(s);
                break;
            }
            return;
        }
    }
    socket_close(s);
    asprinthost(&tmp, addr, addrlen);
    free(addr);
    log_printf(daemon->log, LVL_INFO, "Unexpected server connection from %s",
               tmp);
    free(tmp);
}

static bool daemon_setup_server(daemon_t daemon)
{
    assert(daemon->selector != NULL && daemon->serv_sock < 0);
    daemon->serv_sock = socket_tcp_listen(daemon->bind_server,
                                          daemon->server_port);
    if (daemon->serv_sock >= 0)
    {
        selector_add(daemon->selector, daemon->serv_sock, daemon,
                     daemon_server_accept_cb, NULL);
        return true;
    }
    else
    {
        log_printf(daemon->log, LVL_ERR,
                   "Unable to listen for server connections on %s:%u: %s",
                   daemon->bind_server != NULL ? daemon->bind_server : "*",
                   daemon->server_port,
                   socket_strerror(daemon->serv_sock));
        return false;
    }
}

static bool daemon_setup_remote_server(daemon_t daemon, server_t* srv)
{
    assert(daemon->selector != NULL && srv->host != NULL && srv->sock < 0 && srv->state == CONN_DEAD);
    if (srv->in == NULL)
    {
        srv->in = buf_new(SERVER_BUFFER_IN);
    }
    if (srv->out == NULL)
    {
        srv->out = buf_new(SERVER_BUFFER_OUT);
    }
    srv->state = CONN_CONNECTING;
    srv->sock = socket_tcp_connect2(srv->host, srv->hostlen, false);
    if (srv->sock < 0)
    {
        char* tmp;
        srv->state = CONN_DEAD;
        asprinthost(&tmp, srv->host, srv->hostlen);
        log_printf(daemon->log, LVL_WARN,
                   "Unable to setup remote server (%s) socket: %s",
                   tmp, socket_strerror(srv->sock));
        free(tmp);
        return false;
    }
    selector_add(daemon->selector, srv->sock, srv, daemon_server_incoming_cb,
                 daemon_server_writable_cb);
    return true;
}

bool load_config(daemon_t daemon)
{
    cfg_t cfg;
    const char* log, *bind_multicast, *bind_server, *bind_services;
    const char* bind_tunnelport, *servers;
    int server_port, tunnel_first_port, tunnel_last_port;
    bool update_ssdp = false, update_server = false;
    server_t* server;
    size_t server_cnt;

    if (daemon->cfgfile == NULL)
    {
        daemon->cfgfile = find_config();
    }

    cfg = cfg_open(daemon->cfgfile, daemon->log);
    if (cfg == NULL)
    {
        return false;
    }

    if (!daemon->debug)
    {
        log = cfg_getstr(cfg, "log", "syslog:info");
        if (!log_reopen(daemon->log, log))
        {
            cfg_close(cfg);
            return false;
        }
    }

    bind_multicast = cfg_getstr(cfg, "bind_multicast", NULL);
    if (!valid_bind(daemon->log, "bind_multicast", bind_multicast))
    {
        cfg_close(cfg);
        return false;
    }
    bind_server = cfg_getstr(cfg, "bind_server", NULL);
    if (!valid_bind(daemon->log, "bind_server", bind_server))
    {
        cfg_close(cfg);
        return false;
    }
    bind_services = cfg_getstr(cfg, "bind_services", NULL);
    if (!valid_bind(daemon->log, "bind_services", bind_services))
    {
        cfg_close(cfg);
        return false;
    }
    bind_tunnelport = cfg_getstr(cfg, "bind_tunnels", NULL);
    if (!valid_bind(daemon->log, "bind_tunnels", bind_tunnelport))
    {
        cfg_close(cfg);
        return false;
    }
    server_port = cfg_getint(cfg, "server_port", DEFAULT_PORT);
    if (!valid_port(daemon->log, "server_port", server_port))
    {
        cfg_close(cfg);
        return false;
    }
    tunnel_first_port = cfg_getint(cfg, "first_tunnel_port", DEFAULT_FIRST_TUNNEL_PORT);
    if (!valid_port(daemon->log, "first_tunnel_port", tunnel_first_port))
    {
        cfg_close(cfg);
        return false;
    }
    tunnel_last_port = cfg_getint(cfg, "last_tunnel_port", DEFAULT_LAST_TUNNEL_PORT);
    if (!valid_port(daemon->log, "last_tunnel_port", tunnel_last_port))
    {
        cfg_close(cfg);
        return false;
    }
    if (tunnel_first_port > tunnel_last_port)
    {
        log_printf(daemon->log, LVL_ERR,
                   "Not a valid port given for `last_tunnel_port`: %d",
                   tunnel_last_port);
        return false;
    }
    servers = cfg_getstr(cfg, "servers", NULL);
    if (!valid_servers(daemon, "servers", servers, &server, &server_cnt))
    {
        cfg_close(cfg);
        return false;
    }

    cfg_close(cfg);

    if (safestrcmp(bind_multicast, daemon->bind_multicast) != 0)
    {
        update_ssdp = true;
        free(daemon->bind_multicast);
        daemon->bind_multicast = safestrdup(bind_multicast);
    }

    if (safestrcmp(bind_server, daemon->bind_server) != 0)
    {
        update_server = true;
        free(daemon->bind_server);
        daemon->bind_server = safestrdup(bind_server);
    }

    if (safestrcmp(bind_services, daemon->bind_services) != 0)
    {
        /* TODO: Cause rebinding of current remote service sockets */
        free(daemon->bind_services);
        daemon->bind_services = safestrdup(bind_services);
    }

    if (safestrcmp(bind_tunnelport, daemon->bind_tunnelport) != 0)
    {
        free(daemon->bind_tunnelport);
        daemon->bind_tunnelport = safestrdup(bind_tunnelport);
    }

    if (daemon->tunnel_port_first != tunnel_first_port ||
        daemon->tunnel_port_first + daemon->tunnel_port_count + 1
        != tunnel_last_port)
    {
        size_t nc = (tunnel_last_port - tunnel_first_port) + 1, i;
        if (tunnel_first_port > 0)
        {
            daemon->tunnel_port_first = tunnel_first_port;
            if (nc != daemon->tunnel_port_count)
            {
                size_t in_use = 0, j;
                for (i = 0; i < daemon->tunnel_port_count; ++i)
                {
                    if (daemon->tunnel_port[i].tunnel != NULL)
                    {
                        ++in_use;
                        break;
                    }
                }
                if (nc < in_use)
                {
                    nc = in_use;
                }
                i = 0;
                j = 0;
                for (; in_use > 0; ++j)
                {
                    if (daemon->tunnel_port[j].tunnel != NULL)
                    {
                        daemon->tunnel_port[i] = daemon->tunnel_port[j];
                        ++i;
                        --in_use;
                    }
                }
                daemon->tunnel_port = realloc(daemon->tunnel_port, nc * sizeof(tunnel_port_t));
                memset(daemon->tunnel_port + i, 0,
                       (nc - i) * sizeof(tunnel_port_t));
            }
        }
        else
        {
            bool all_empty = true;
            for (i = 0; i < daemon->tunnel_port_count; ++i)
            {
                if (daemon->tunnel_port[i].tunnel != NULL)
                {
                    all_empty = false;
                    break;
                }
            }
            if (all_empty)
            {
                daemon->tunnel_port_count = 0;
                free(daemon->tunnel_port);
                daemon->tunnel_port = NULL;
            }
            daemon->tunnel_port_first = 0;
        }
    }

    if (server_port != daemon->server_port)
    {
        update_server = true;
        daemon->server_port = (uint16_t)server_port;
    }

    if (update_ssdp && daemon->ssdp != NULL)
    {
        ssdp_free(daemon->ssdp);
        daemon->ssdp = NULL;
        daemon_setup_ssdp(daemon);
    }
    if (update_server && daemon->serv_sock >= 0)
    {
        selector_remove(daemon->selector, daemon->serv_sock);
        socket_close(daemon->serv_sock);
        daemon->serv_sock = -1;
        daemon_setup_server(daemon);
    }

    {
        size_t i, j, oldcnt = daemon->servers;

        for (i = daemon->servers; i > 0; --i)
        {
            bool found = false;
            server_t* srv = daemon->server + (i - 1);
            for (j = server_cnt; j > 0; --j)
            {
                if (socket_samehostandport(srv->host, srv->hostlen,
                                           server[j - 1].host,
                                           server[j - 1].hostlen))
                {
                    found = true;
                    server_free2(server + (j - 1));
                    --server_cnt;
                    memmove(server + j - 1, server + j,
                            (server_cnt - (j - 1)) * sizeof(server_t));
                    break;
                }
            }
            if (!found)
            {
                server_free(daemon, srv);
            }
        }
        if (server_cnt > 0)
        {
            size_t newcnt = daemon->servers + server_cnt;
            if (newcnt > oldcnt)
            {
                daemon->server = realloc(daemon->server,
                                         newcnt * sizeof(server_t));
            }
            memcpy(daemon->server + daemon->servers,
                   server, server_cnt * sizeof(server_t));
            i = daemon->servers;
            daemon->servers = newcnt;
            if (daemon->selector != NULL)
            {
                for (; i < daemon->servers; ++i)
                {
                    daemon_setup_remote_server(daemon, daemon->server + i);
                }
            }
        }
        free(server);
        server = NULL;
        server_cnt = 0;
    }

    return true;
}

void free_daemon(daemon_t daemon)
{
    if (daemon->tunnel_port_first > 0)
    {
        size_t i;
        for (i = 0; i < daemon->tunnel_port_count; ++i)
        {
            if (daemon->tunnel_port[i].sock >= 0)
            {
                selector_remove(daemon->selector, daemon->tunnel_port[i].sock);
                socket_close(daemon->tunnel_port[i].sock);
            }
        }
        free(daemon->tunnel_port);
        daemon->tunnel_port_first = 0;
    }
    if (daemon->serv_sock >= 0)
    {
        selector_remove(daemon->selector, daemon->serv_sock);
        socket_close(daemon->serv_sock);
    }
    while (daemon->servers > 0)
    {
        server_free(daemon, daemon->server + daemon->servers - 1);
    }
    free(daemon->server);
    map_free(daemon->locals);
    map_free(daemon->remotes);
    ssdp_free(daemon->ssdp);
    selector_free(daemon->selector);
    timers_free(daemon->timers);
    log_close(daemon->log);
    free(daemon->ssdp_s);
    uuid_clear(daemon->uuid);
    free(daemon->bind_multicast);
    free(daemon->bind_server);
    free(daemon->cfgfile);
}

static bool daemon_quit = false, daemon_reload = false;

void daemon_quit_cb(int signum)
{
    daemon_quit = true;
}

void daemon_reload_cb(int signum)
{
    daemon_reload = true;
}

static char* daemon_generate_uid(daemon_t daemon)
{
    char* uid = calloc(45, 1);
    if (uuid_is_null(daemon->uuid))
    {
        const char* dir = getenv("XDG_CACHE_HOME");
        char* tmp2 = NULL;
        char* tmp = NULL;
        if (dir == NULL || *dir == '\0')
        {
            const char* home = getenv("HOME");
            if (home == NULL || *home == '\0')
            {
                struct passwd* pw = getpwuid(getuid());
                if (pw != NULL)
                {
                    home = pw->pw_dir;
                }
            }
            if (home != NULL && *home != '\0')
            {
                asprintf(&tmp2, "%s/.cache", home);
                dir = tmp2;
            }
        }
        if (dir != NULL && *dir != '\0')
        {
            asprintf(&tmp, "%s/upnpproxy.cache", dir);
            free(tmp2);
        }
        if (tmp != NULL)
        {
            FILE* fh = fopen(tmp, "rt");
            if (fh != NULL)
            {
                char* line = NULL;
                size_t linelen = 0;
                int ret;
                while ((ret = getline(&line, &linelen, fh)) != -1)
                {
                    char* in;
                    if (line[ret] != '\0')
                        line[ret] = '\0';
                    in = trim(line);
                    if (*in == '\0')
                        continue;
                    if (uuid_parse(in, daemon->uuid) == 0)
                    {
                        break;
                    }
                }
                free(line);
                fclose(fh);
            }
        }

        if (uuid_is_null(daemon->uuid))
        {
            uuid_generate(daemon->uuid);

            if (tmp != NULL)
            {
                FILE* fh = fopen(tmp, "wt");
                if (fh == NULL)
                {
                    char* pos = strrchr(tmp, '/');
                    if (pos != NULL)
                    {
                        *pos = '\0';
                        if (mkdir_p(tmp))
                        {
                            *pos = '/';
                            fh = fopen(tmp, "wt");
                        }
                    }
                }
                if (fh != NULL)
                {
                    uuid_unparse_lower(daemon->uuid, uid);
                    fputs(uid, fh);
                    fputs("\n\n", fh);
                    fclose(fh);
                }
                else
                {
                    log_printf(daemon->log, LVL_WARN, "Unable to save generated UUID to `%s`: %s", tmp, strerror(errno));
                }
            }
        }

        free(tmp);
    }
    memcpy(uid, "uuid:", 5);
    uuid_unparse_lower(daemon->uuid, uid + 5);
    return uid;
}

int run_daemon(daemon_t daemon)
{
    size_t i;
    daemon->selector = selector_new();
    if (daemon->selector == NULL)
    {
        log_puts(daemon->log, LVL_ERR, "Unable to create selector");
        return EXIT_FAILURE;
    }
    daemon->timers = timers_new();
    if (daemon->timers == NULL)
    {
        log_puts(daemon->log, LVL_ERR, "Unable to create timers");
        return EXIT_FAILURE;
    }

    daemon->ssdp_s = daemon_generate_uid(daemon);
    daemon->locals = map_new(sizeof(struct _localservice_t), localservice_hash,
                             localservice_eq, localservice_free);
    daemon->remotes = map_new(sizeof(struct _remoteservice_t),
                              remoteservice_hash,
                              remoteservice_eq, remoteservice_free);

    if (!daemon_setup_server(daemon))
    {
        return EXIT_FAILURE;
    }
    if (!daemon_setup_ssdp(daemon))
    {
        return EXIT_FAILURE;
    }

    for (i = 0; i < daemon->servers; ++i)
    {
        daemon_setup_remote_server(daemon, daemon->server + i);
    }

    signal(SIGINT, daemon_quit_cb);
    signal(SIGTERM, daemon_quit_cb);
    signal(SIGQUIT, daemon_quit_cb);
    signal(SIGHUP, daemon_reload_cb);
    signal(SIGPIPE, SIG_IGN);

    for (;;)
    {
        unsigned long timeout_ms;

        if (daemon_quit)
        {
            log_puts(daemon->log, LVL_INFO, "Caught INT/TERM/QUIT signal, so quitting");
            break;
        }
        if (daemon_reload)
        {
            log_puts(daemon->log, LVL_INFO, "Caught HUP signal, so reloading config");
            load_config(daemon);
            daemon_reload = false;
        }

        timeout_ms = timers_tick(daemon->timers);
        if (timeout_ms == 0)
        {
            /* Default timeout, 2 hours */
            timeout_ms = 2 * 60 * 60 * 1000;
        }

        if (!selector_tick(daemon->selector, timeout_ms))
        {
            log_printf(daemon->log, LVL_ERR, "Selector failed: %s",
                       strerror(errno));
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

void server_init(daemon_t daemon, server_t* srv,
                 struct sockaddr* host, socklen_t hostlen)
{
    memset(srv, 0, sizeof(server_t));
    srv->daemon = daemon;
    srv->host = host;
    srv->hostlen = hostlen;
    srv->sock = -1;
    srv->local_tunnels = map_new(sizeof(tunnel_t), local_tunnel_hash,
                                 local_tunnel_eq, local_tunnel_free);
    srv->remote_tunnels = map_new(sizeof(tunnel_t), remote_tunnel_hash,
                                  remote_tunnel_eq, remote_tunnel_free);
    srv->waiting_pkgs = vector_new(sizeof(pkg_t*));
}

void server_free2(server_t* srv)
{
    if (srv->reconnect_timecb != NULL)
    {
        timecb_cancel(srv->reconnect_timecb);
        srv->reconnect_timecb = NULL;
    }
    if (srv->sock >= 0)
    {
        selector_remove(srv->daemon->selector, srv->sock);
        socket_close(srv->sock);
        srv->sock = -1;
    }
    map_free(srv->local_tunnels);
    map_free(srv->remote_tunnels);
    if (srv->waiting_pkgs != NULL)
    {
        size_t i;
        for (i = 0; i < vector_size(srv->waiting_pkgs); ++i)
        {
            pkg_free(*((pkg_t**)vector_get(srv->waiting_pkgs, i)));
        }
        vector_free(srv->waiting_pkgs);
    }
    buf_free(srv->in);
    buf_free(srv->out);
    free(srv->host);
}

void server_free(daemon_t daemon, server_t* srv)
{
    size_t idx;
    if (srv->reconnect_timecb != NULL)
    {
        timecb_cancel(srv->reconnect_timecb);
        srv->reconnect_timecb = NULL;
    }
    if (srv->sock >= 0)
    {
        if (srv->state == CONN_CONNECTED)
        {
            daemon_clear_remotes(daemon, srv);
        }
        selector_remove(daemon->selector, srv->sock);
        socket_close(srv->sock);
    }
    server_free2(srv);

    idx = srv - daemon->server;
    assert(idx < daemon->servers);
    --daemon->servers;
    memmove(daemon->server + idx, daemon->server + idx + 1,
            (daemon->servers - idx) * sizeof(server_t));
}

uint32_t localservice_hash(const void* _local)
{
    return ((localservice_t*)_local)->id;
}

bool localservice_eq(const void* _l1, const void* _l2)
{
    return ((localservice_t*)_l1)->id == ((localservice_t*)_l2)->id;
}

void localservice_free(void* _local)
{
    localservice_t* local = _local;
    if (local->expirecb != NULL)
    {
        timecb_cancel(local->expirecb);
        local->expirecb = NULL;
    }
    if (local->daemon != NULL)
    {
        /* Tell all connected servers about the loss */
        pkg_t pkg;
        size_t i;
        pkg_old_service(&pkg, local->id);
        for (i = 0; i < local->daemon->servers; ++i)
        {
            daemon_server_write_pkg(local->daemon->server + i, &pkg, true);
        }
    }
    free(local->host);
    free(local->usn);
    free(local->location);
    free(local->service);
    free(local->server);
    free(local->opt);
    free(local->nls);
}

uint32_t remoteservice_hash(const void* _remote)
{
    /* TODO: Improve hash with ->source in some way */
    return ((remoteservice_t*)_remote)->source_id;
}

bool remoteservice_eq(const void* _r1, const void* _r2)
{
    return ((remoteservice_t*)_r1)->source_id == ((remoteservice_t*)_r2)->source_id &&
        ((remoteservice_t*)_r1)->source == ((remoteservice_t*)_r2)->source;
}

void remoteservice_free(void* _remote)
{
    remoteservice_t* remote = _remote;
    daemon_t daemon = remote->source->daemon;

    if (remote->touchcb != NULL)
    {
        timecb_cancel(remote->touchcb);
        remote->touchcb = NULL;
    }

    if (daemon->ssdp != NULL && remote->notify.host != NULL)
    {
        ssdp_byebye(daemon->ssdp, &(remote->notify));
    }
    if (remote->sock >= 0)
    {
        selector_remove(daemon->selector, remote->sock);
        socket_close(remote->sock);
    }

    free(remote->notify.host);
    free(remote->notify.location);
    free(remote->notify.server);
    free(remote->notify.usn);
    free(remote->notify.nt);
    free(remote->notify.opt);
    free(remote->notify.nls);
    free(remote->host);
}

static int _daemon_server_flush_output(server_t* server)
{
    size_t avail;
    const char* ptr;
    ssize_t got;
    for (;;)
    {
        ptr = buf_rptr(server->out, &avail);
        if (avail == 0)
        {
            return 0;
        }
        got = socket_write(server->sock, ptr, avail);
        if (got <= 0)
        {
            char* tmp;
            if (socket_blockingerror(server->sock))
            {
                return 1;
            }

            asprinthost(&tmp, server->host, server->hostlen);
            log_printf(server->daemon->log, LVL_WARN,
                       "Lost connection with server %s: %s", tmp,
                       socket_strerror(server->sock));
            free(tmp);

            daemon_lost_server(server->daemon, server, false);
            return -1;
        }
        buf_rmove(server->out, got);
    }
}

static void daemon_server_flush_output(server_t* server)
{
    if (_daemon_server_flush_output(server) > 0)
    {
        selector_chkwrite(server->daemon->selector, server->sock, true);
    }
}

static void daemon_server_write_pkg(server_t* server, pkg_t* pkg, bool flush)
{
    unsigned char try;

    if (server->state == CONN_DEAD)
    {
        return;
    }

    for (try = 0; try < 2; ++try)
    {
        if (pkg_write(server->out, pkg))
        {
            if (flush)
            {
                daemon_server_flush_output(server);
            }
            return;
        }

        if (try == 0)
        {
            daemon_server_flush_output(server);
        }
        else
        {
            pkg_t* pkgcpy = pkg_dup(pkg);
            if (pkgcpy != NULL)
            {
                vector_push(server->waiting_pkgs, &pkgcpy);
            }
            return;
        }
    }
}

static long daemon_remoteservice_touch(void* userdata)
{
    remoteservice_t* remote = userdata;

    if (remote->source->daemon->ssdp != NULL)
    {
        remote->notify.expires = time(NULL) + REMOTE_EXPIRE_TTL;
        ssdp_notify(remote->source->daemon->ssdp, &(remote->notify));
    }

    return 0;
}

static long daemon_localservice_expire(void* userdata)
{
    localservice_t* local = userdata;
    local->expirecb = NULL;
    map_remove(local->daemon->locals, local);
    return -1;
}
