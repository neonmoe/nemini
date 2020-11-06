// The openssl code here is largely based on sircmpwn's gmni, thanks!
// => https://git.sr.ht/~sircmpwn/gmni

#include <openssl/ssl.h>
#include <string.h>
#include <SDL.h>
#include "error.h"
#include "socket.h"
#include "url.h"
#include "gemini.h"

static SSL_CTX *ctx;

int ssl_cert_verify_callback(X509_STORE_CTX *x509_store_ctx, void *arg) {
    // TODO: Implement TOFU cert verification
    return 1;
}

enum nemini_error net_init(void) {
    ctx = SSL_CTX_new(TLS_method());
    if (ctx == NULL) { return ERR_SSL_CTX_INIT; }
    if (SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) != 1) { return ERR_SSL_TLS_MIN_FAILED; }
    SSL_CTX_set_cert_verify_callback(ctx, ssl_cert_verify_callback, NULL);
    return ERR_NONE;
}

void net_free(void) {
    SSL_CTX_free(ctx);
}

enum nemini_error net_request(const char *url, struct gemini_response *result) {
    char *host, *port, *resource;
    int parse_status = parse_gemini_url(url, &host, &port, &resource);
    if (parse_status != ERR_NONE) { return parse_status; }

    int fd;
    int socket_status = create_socket(host, port, &fd);
    if (socket_status != ERR_NONE) { return socket_status; }

    SSL *ssl = SSL_new(ctx);
    if (ssl == NULL) { return ERR_UNEXPECTED; }

    SSL_set_connect_state(ssl);
    if (SSL_set1_host(ssl, host) != 1||
        SSL_set_tlsext_host_name(ssl, host) != 1 ||
        SSL_set_fd(ssl, fd) != 1 ||
        SSL_connect(ssl) != 1) {

        SSL_free(ssl);
        free(resource);
        free(port);
        free(host);
        return ERR_SSL_CONNECT;
    }

    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert == NULL) {
        SSL_free(ssl);
        free(resource);
        free(port);
        free(host);
        return ERR_SSL_CERT_MISSING;
    }

    long verification = SSL_get_verify_result(ssl);
    if (verification != X509_V_OK) {
        SSL_free(ssl);
        free(resource);
        free(port);
        free(host);
        return ERR_SSL_CERT_VERIFY;
    }

    BIO *bio_ssl = BIO_new(BIO_f_ssl());
    if (bio_ssl == NULL) { return ERR_UNEXPECTED; }

    BIO *bio_buffered = BIO_new(BIO_f_buffer());
    if (bio_buffered == NULL) { return ERR_UNEXPECTED; }

    BIO_set_ssl(bio_ssl, ssl, 0);
    BIO_push(bio_buffered, bio_ssl);

    int url_length = strlen(url);
    char *request = malloc(url_length + 3);
    memcpy(request, url, url_length);
    memcpy(request + url_length, "\r\n\0", 3);
    if (BIO_puts(bio_ssl, request) == -1) {
        BIO_free(bio_buffered);
        BIO_free(bio_ssl);
        SSL_free(ssl);
        free(resource);
        free(port);
        free(host);
        return ERR_PUT_REQUEST;
    }

    // Length: <STATUS><SPACE><META><CR><LF><NUL>
    char gemini_header[2 + 1 + 1024 + 1 + 1 + 1] = {0};
    int header_bytes = BIO_gets(bio_buffered, gemini_header, sizeof(gemini_header));
    if (header_bytes == -1) {
        BIO_free(bio_buffered);
        BIO_free(bio_ssl);
        SSL_free(ssl);
        free(resource);
        free(port);
        free(host);
        return ERR_GET_HEADER;
    }

    struct gemini_response res = {0};
    res.status = (gemini_header[0] - '0') * 10 + (gemini_header[1] - '0');
    if (!is_valid_gemini_header(gemini_header, header_bytes)) {
        BIO_free(bio_buffered);
        BIO_free(bio_ssl);
        SSL_free(ssl);
        free(resource);
        free(port);
        free(host);
        return ERR_OUT_OF_MEMORY;
    }
    int meta_length = header_bytes - 2 - 1 - 2;
    char *meta = malloc(meta_length + 1);
    if (meta == NULL) {
        BIO_free(bio_buffered);
        BIO_free(bio_ssl);
        SSL_free(ssl);
        free(resource);
        free(port);
        free(host);
        return ERR_OUT_OF_MEMORY;
    }
    memcpy(meta, gemini_header + 3, meta_length);
    meta[meta_length] = '\0';
    res.meta.meta = meta;

    if (gemini_header[0] == '2') {
        void *body = NULL;
        int body_length = 0;
        int written_length = 0;
        int bytes_read;
        do {
            if (written_length >= body_length) {
                if (body_length == 0) {
                    body_length = 128;
                } else {
                    body_length *= 2;
                }
                void *prev_body = body;
                body = realloc(body, body_length);
                if (body == NULL) {
                    free(prev_body);
                    free(meta);
                    BIO_free(bio_buffered);
                    BIO_free(bio_ssl);
                    SSL_free(ssl);
                    free(resource);
                    free(port);
                    free(host);
                    return ERR_OUT_OF_MEMORY;
                }
            }
            void *cursor = (void *)(&((char *)body)[written_length]);
            bytes_read = BIO_read(bio_buffered, cursor,
                                  body_length - written_length);
            if (bytes_read >= 0) {
                written_length += bytes_read;
            }
        } while (bytes_read > 0);
        if (bytes_read == 0) {
            if (written_length >= body_length || written_length < 0) {
                // Before reading, the memory is reallocated if the
                // buffer is full, so it should never be full at this
                // point. And it obviously cannot be negative.
                return ERR_UNEXPECTED;
            }
            res.body = (char *) body;
            res.body[written_length] = '\0';
        } else {
            free(body);
            free(meta);
            BIO_free(bio_buffered);
            BIO_free(bio_ssl);
            SSL_free(ssl);
            free(resource);
            free(port);
            free(host);
            return ERR_GET_BODY;
        }
    } else {
        res.body = NULL;
    }

    // res.body and res.meta.meta are allocated in this function, but outlive it.
    // They are free with gemini_response_free();

    BIO_free(bio_buffered);
    BIO_free(bio_ssl);
    SSL_free(ssl);
    free(resource);
    free(port);
    free(host);

    *result = res;
    return ERR_NONE;
}
