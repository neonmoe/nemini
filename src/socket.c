#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>

#include "error.h"

enum nemini_error create_socket(const char *url, int *fd) {
    struct addrinfo hints;
    SDL_Log("start create_socket");
    memset(&hints, 0, sizeof(hints));
    SDL_Log("memsetted");
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = IPPROTO_TCP;

    SDL_Log("hints setup");
    struct addrinfo *info;
    const char *host = "localhost";
    const char *port = "1965";
    SDL_Log("calling getaddrinfo");
    int status = getaddrinfo(host, port, &hints, &info);
    if (status != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "getaddrinfo() failed for %s:%s: %s",
                     host, port, gai_strerror(status));
        return ERR_DNS_RESOLUTION;
    }

    SDL_Log("starting to loop");
    int socket_fd = -1;
    struct addrinfo *rp;
    for (rp = info; rp != NULL; rp = rp->ai_next) {
        SDL_Log("processing rp %d", info);
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socket_fd == -1) {
            continue;
        }

        if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
            SDL_Log("connected to socket: %d", socket_fd);
            break;
        }

        close(socket_fd);
        socket_fd = -1;
    }

    freeaddrinfo(info);

    if (socket_fd != -1) {
        SDL_Log("socket_fd: %d", socket_fd);
        *fd = socket_fd;
        return ERR_NONE;
    } else {
        SDL_Log("Couldn't find socket");
        return ERR_CONNECT;
    }
}
