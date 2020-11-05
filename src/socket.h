#ifndef NEMINI_SOCKET_H
#define NEMINI_SOCKET_H

#include "error.h"

enum nemini_error create_socket(const char *url, int *fd);

#endif
