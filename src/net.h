#ifndef NET_H
#define NET_H

#include "error.h"

void net_init(void);
void net_free(void);
enum nemini_error net_request(const char *url);

#endif
