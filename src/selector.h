#ifndef SELECTOR_H
#define SELECTOR_H

typedef struct _selector_t* selector_t;

#include "socket.h"

typedef void (* read_callback_t)(void* userdata, socket_t sock);
typedef void (* write_callback_t)(void* userdata, socket_t sock);

selector_t selector_new(void);

void selector_add(selector_t selector, socket_t sock,
                  void* userdata,
                  read_callback_t read_callback,
                  write_callback_t write_callback);

/* This function does not check if you set a write_callback back when calling
 * add. If you set check_write to true and then have write_callback == NULL
 * you just caused a segfault. */
void selector_chk(selector_t selector, socket_t sock,
                  bool check_read, bool check_write);
void selector_chkread(selector_t selector, socket_t sock,
                      bool check_read);
void selector_chkwrite(selector_t selector, socket_t sock,
                       bool check_write);

void selector_remove(selector_t selector, socket_t sock);

void selector_free(selector_t selector);

/* timeout_ms == 0 means no timeout */
bool selector_tick(selector_t selector, unsigned long timeout_ms);

#endif /* SELECTOR_H */
