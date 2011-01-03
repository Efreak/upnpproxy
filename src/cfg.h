#ifndef CFG_H
#define CFG_H

typedef struct _cfg_t* cfg_t;

#include "log.h"

cfg_t cfg_open(const char* filename, log_t log);
void cfg_close(cfg_t cfg);

const char* cfg_getstr(cfg_t cfg, const char* key, const char* _default);
int cfg_getint(cfg_t cfg, const char* key, int _default);

#endif /* CFG_H */
