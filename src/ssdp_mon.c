#include "common.h"

#include <stdio.h>
#include <signal.h>

#include "ssdp.h"
#include "log.h"

static void search_cb(void* userdata, ssdp_search_t* search);
static void search_resp_cb(void* userdata, ssdp_search_t* search,
                           ssdp_notify_t* notify);
static void notify_cb(void* userdata, ssdp_notify_t* notify);
static void quit_cb(int signum);
static bool quit = false;

int main(int argc, char** argv)
{
    selector_t selector;
    ssdp_t ssdp;
    log_t log;
    if (argc > 1)
    {
        fputs("ssdp_mon: Expects no arguments.\n", stderr);
        return EXIT_FAILURE;
    }
    log = log_open();
    selector = selector_new();
    if (selector == NULL)
    {
        fputs("ssdp_mon: Failed to create selector.\n", stderr);
        log_close(log);
        return EXIT_FAILURE;
    }
    ssdp = ssdp_new(log, selector, NULL, NULL, search_cb, search_resp_cb, notify_cb);
    if (ssdp == NULL)
    {
        fputs("ssdp_mon: Failed to setup SSDP.\n", stderr);
        selector_free(selector);
        log_close(log);
        return EXIT_FAILURE;
    }

    signal(SIGINT, quit_cb);
    signal(SIGTERM, quit_cb);
    signal(SIGHUP, quit_cb);

    while (!quit)
    {
        if (!selector_tick(selector, 1000))
        {
            fputs("ssdp_mon: Selector failed.\n", stderr);
            ssdp_free(ssdp);
            selector_free(selector);
            log_close(log);
            return EXIT_FAILURE;
        }
    }

    ssdp_free(ssdp);
    selector_free(selector);
    log_close(log);
    return EXIT_SUCCESS;
}

void quit_cb(int signum)
{
    quit = true;
}

void search_cb(void* userdata, ssdp_search_t* search)
{
    char* tmp;
    fputs("*** Search request\n", stdout);
    if (search->s != NULL)
        fprintf(stdout, "* S: %s\n", search->s);
    asprinthost(&tmp, search->host, search->hostlen);
    fprintf(stdout, "* Host: %s\n", tmp);
    free(tmp);
    if (search->st != NULL)
        fprintf(stdout, "* ST: %s\n", search->st);
}

void search_resp_cb(void* userdata, ssdp_search_t* search, ssdp_notify_t* notify)
{
    char* tmp;
    fputs("*** Search response\n", stdout);
    if (search->s != NULL)
        fprintf(stdout, "* S: %s\n", search->s);
    if (search->st != NULL)
        fprintf(stdout, "* ST: %s\n", search->st);
    if (notify->location != NULL)
        fprintf(stdout, "* Location: %s\n", notify->location);
    if (notify->usn != NULL)
        fprintf(stdout, "* USN: %s\n", notify->usn);
    tmp = malloc(256);
    strftime(tmp, 256, "%a, %d %b %Y %H:%M:%S %z", localtime(&notify->expires));
    fprintf(stdout, "* Expires: %s\n", tmp);
    free(tmp);
}

void notify_cb(void* userdata, ssdp_notify_t* notify)
{
    char* tmp;
    fputs("*** Notify request\n", stdout);
    asprinthost(&tmp, notify->host, notify->hostlen);
    fprintf(stdout, "* Host: %s\n", tmp);
    free(tmp);
    if (notify->location != NULL)
        fprintf(stdout, "* Location: %s\n", notify->location);
    if (notify->server != NULL)
        fprintf(stdout, "* Server: %s\n", notify->server);
    if (notify->usn != NULL)
        fprintf(stdout, "* USN: %s\n", notify->usn);
    if (notify->nt != NULL)
        fprintf(stdout, "* NT: %s\n", notify->nt);
    if (notify->nts != NULL)
        fprintf(stdout, "* NTS: %s\n", notify->nts);
    tmp = malloc(256);
    strftime(tmp, 256, "%a, %d %b %Y %H:%M:%S %z", localtime(&notify->expires));
    fprintf(stdout, "* Expires: %s\n", tmp);
    free(tmp);
}
