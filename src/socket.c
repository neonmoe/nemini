#if _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include "error.h"

enum nemini_error sockets_init(void) {
#if _WIN32
    static WSADATA wsaData;
    int status = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (status != 0) {
        return ERR_WSA_STARTUP;
    }
#endif
    return ERR_NONE;
}

void sockets_free(void) {
#if _WIN32
    WSACleanup();
#endif
}

void socket_shutdown(int fd) {
#if _WIN32
    shutdown(fd, SD_BOTH);
    closesocket(fd);
#else
    shutdown(fd, SHUT_RDWR);
#endif
}

enum nemini_error socket_connect(const char *host, const char *port, int *fd) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *info;
    int status = getaddrinfo(host, port, &hints, &info);
    if (status != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "getaddrinfo() failed for %s:%s: %s",
                     host, port, gai_strerror(status));
        return ERR_DNS_RESOLUTION;
    }

    int socket_fd = -1;
    struct addrinfo *rp;
    for (rp = info; rp != NULL; rp = rp->ai_next) {
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socket_fd == -1) {
            continue;
        }

        if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
            // Ok!
            break;
        }

        close(socket_fd);
        socket_fd = -1;
    }

    freeaddrinfo(info);

    if (socket_fd != -1) {
        *fd = socket_fd;
        return ERR_NONE;
    } else {
        return ERR_CONNECT;
    }
}
