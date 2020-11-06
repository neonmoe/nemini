#ifndef NEMINI_SOCKET_H
#define NEMINI_SOCKET_H

#include "error.h"

enum nemini_error sockets_init(void);
void sockets_free(void);
enum nemini_error socket_connect(const char *host, const char *port, int *fd);
void socket_shutdown(int fd);

#endif
