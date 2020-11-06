#ifndef NEMINI_SOCKET_H
#define NEMINI_SOCKET_H

#include "error.h"

enum nemini_error socket_create(const char *host, const char *port, int *fd);

#endif
