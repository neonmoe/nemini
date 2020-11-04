#include <SDL.h>
#include "error.h"

// First, platform-specific socket implementations, after these is the juicy TLS part.

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>

static enum nemini_error create_socket(const char *url, int *fd) {
    struct addrinfo hints;
    SDL_Log("start create_socket");
    memset(&hints, 0, sizeof(hints));
    SDL_Log("memsetted");
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;//IPPROTO_TCP; // or 0?

    SDL_Log("hints setup");
    struct addrinfo *info;
    const char *host = "localhost";
    const char *port = "1965";
    SDL_Log("calling getaddrinfo");
    int status = getaddrinfo(host, port, &hints, &info);
    if (!status) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "getaddrinfo() failed for %s:%s: %s",
                     host, port, gai_strerror(status));
        return ERR_DNS_RESOLUTION;
    }

    SDL_Log("starting to loop");
    int sfd = -1;
    struct addrinfo *rp;
    for (rp = info; rp != NULL; rp = rp->ai_next) {
        SDL_Log("processing rp %d", info);
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) {
            continue;
        }

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            SDL_Log("connected to sfd: %d", sfd);
            break;
        }

        close(sfd);
        sfd = -1;
    }

    freeaddrinfo(info);

    if (sfd != -1) {
        SDL_Log("sfd: %d", sfd);
        *fd = sfd;
        return ERR_NONE;
    } else {
        SDL_Log("Couldn't find socket");
        return ERR_CONNECT;
    }
}

// The openssl code here is largely based on sircmpwn's gmni, thanks!
// => https://git.sr.ht/~sircmpwn/gmni

#include <openssl/ssl.h>
#include <string.h>

static SSL_CTX *ctx;

int ssl_cert_verify_callback(X509_STORE_CTX *x509_store_ctx, void *arg) {
    // TODO: Implement TOFU cert verification
    return 1;
}

void net_init(void) {
    ctx = SSL_CTX_new(TLS_method());
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_cert_verify_callback(ctx, ssl_cert_verify_callback, NULL);
}

void net_free(void) {
    SSL_CTX_free(ctx);
}

enum nemini_error net_request(const char *url) {
    int fd;
    int socket_status = create_socket("127.0.0.1", &fd);
    if (socket_status != ERR_NONE) { return socket_status; }

    SSL *ssl = SSL_new(ctx);
    if (!ssl) { return ERR_UNEXPECTED; }

    SSL_set_connect_state(ssl);
    if (!SSL_set1_host(ssl, "localhost") ||
        !SSL_set_tlsext_host_name(ssl, "localhost") ||
        !SSL_set_fd(ssl, fd) ||
        !SSL_connect(ssl)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SSL_connect() failed");

        SSL_free(ssl);
        return ERR_SSL_CONNECT;
    }

    X509 *cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "certificate missing");

        SSL_free(ssl);
        return ERR_SSL_CERT_MISSING;
    }

    long verification = SSL_get_verify_result(ssl);
    if (verification != X509_V_OK) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "certificate verification failed");

        SSL_free(ssl);
        return ERR_SSL_CERT_VERIFY;
    }

    BIO *bio_ssl = BIO_new(BIO_f_ssl());
    if (!bio_ssl) { return ERR_UNEXPECTED; }

    BIO *bio_buffered = BIO_new(BIO_f_buffer());
    if (!bio_buffered) { return ERR_UNEXPECTED; }

    BIO_set_ssl(bio_ssl, ssl, 0);
    BIO_push(bio_buffered, bio_ssl);

    int url_length = strlen(url);
    char *request = malloc(url_length + 3);
    memcpy(request, url, url_length);
    memcpy(request + url_length, "\r\n\0", 3);
    if (BIO_puts(bio_ssl, request) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "error sending request: %s", request);

        BIO_free(bio_buffered);
        BIO_free(bio_ssl);
        SSL_free(ssl);
        return ERR_PUT_REQUEST;
    }

    // Length: <STATUS><SPACE><META><CR><LF><NUL>
    char gemini_header[2 + 1 + 1024 + 1 + 1 + 1] = {0};
    int header_bytes = BIO_gets(bio_buffered, gemini_header, sizeof(gemini_header));
    if (header_bytes == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "error receiving header");

        BIO_free(bio_buffered);
        BIO_free(bio_ssl);
        SSL_free(ssl);
        return ERR_GET_HEADER;
    }

    BIO_free(bio_buffered);
    BIO_free(bio_ssl);
    SSL_free(ssl);

    SDL_Log("Connection successfully established and closed. Response:\n%s\n%s", gemini_header, "");

    return ERR_NONE;
}
