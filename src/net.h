#ifndef NEMINI_NET_H
#define NEMINI_NET_H

#include "error.h"
#include "gemini.h"

enum nemini_error net_init(void);
void net_free(void);
enum nemini_error net_request(const char *url, struct gemini_response *result);

#endif
