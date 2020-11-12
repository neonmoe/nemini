// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

// The openssl code here is largely based on sircmpwn's gmni, thanks!
// => https://git.sr.ht/~sircmpwn/gmni
// Nothing has been copied verbatim, but my knowledge of "which ssl.h
// functions need to be called to open a pipe to throw bytes at" was
// gathered from reading gmni's source.

#include <openssl/ssl.h>
#include <SDL.h>
#include "error.h"
#include "socket.h"
#include "url.h"
#include "gemini.h"
#include "browser.h"

static SSL_CTX *ctx;

int ssl_cert_verify_callback(X509_STORE_CTX *ctx, void *arg) {
    long err = X509_V_OK;
    // This returns the cert which we're verifying, and isn't supposed
    // to be freed (unlike the one in net_request).
    X509 *cert = X509_STORE_CTX_get0_cert(ctx);

    // TODO: Implement TOFU verification

free_cert_and_return:
    X509_STORE_CTX_set_error(ctx, err);
    if (err == X509_V_OK) {
        return 1;
    } else {
        return 0;
    }
}

enum nemini_error net_init(void) {
    ctx = SSL_CTX_new(TLS_method());
    if (ctx == NULL) { return ERR_SSL_CTX_INIT; }
    SSL_CTX_set_cert_verify_callback(ctx, ssl_cert_verify_callback, NULL);
    if (SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) != 1) { return ERR_SSL_TLS_MIN_FAILED; }

    return ERR_NONE;
}

void net_free(void) {
    SSL_CTX_free(ctx);
}

enum nemini_error net_request(const char *url, struct gemini_response *result) {
    enum nemini_error err = ERR_NONE;

    browser_set_status(LOADING_CONNECTING);

    char *host, *port, *resource;
    int parse_status = parse_gemini_url(url, &host, &port, &resource);
    if (parse_status != ERR_NONE) { return parse_status; }

    int socket_fd;
    int socket_status = socket_connect(host, port, &socket_fd);
    if (socket_status != ERR_NONE) {
        err = socket_status;
        goto free_up_to_url;
    }

    browser_set_status(LOADING_ESTABLISHING_TLS);

    SSL *ssl = SSL_new(ctx);
    if (ssl == NULL) { return ERR_UNEXPECTED; }

    SSL_set_connect_state(ssl);
    if (SSL_set_tlsext_host_name(ssl, host) != 1 ||
        SSL_set1_host(ssl, host) != 1 ||
        SSL_set_fd(ssl, socket_fd) != 1 ||
        SSL_connect(ssl) != 1) {
        err = ERR_SSL_CONNECT;
        goto free_up_to_ssl;
    }

    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert == NULL) {
        err = ERR_SSL_CERT_MISSING;
        goto free_up_to_ssl;
    }
    X509_free(cert);

    long verify_result = SSL_get_verify_result(ssl);
    if (verify_result != X509_V_OK) {
        err = ERR_SSL_CERT_VERIFY;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "certificate verification failed for %s: %s",
                     host, X509_verify_cert_error_string(verify_result));
        goto free_up_to_ssl;
    }

    BIO *bio_ssl = BIO_new(BIO_f_ssl());
    if (bio_ssl == NULL) { return ERR_UNEXPECTED; }

    BIO *bio_buffered = BIO_new(BIO_f_buffer());
    if (bio_buffered == NULL) { return ERR_UNEXPECTED; }

    BIO_set_ssl(bio_ssl, ssl, 0);
    BIO_push(bio_buffered, bio_ssl);

    browser_set_status(LOADING_SENDING_REQUEST);

    int url_length = SDL_strlen(url);
    char *request = SDL_malloc(url_length + 3);
    SDL_memcpy(request, url, url_length);
    SDL_memcpy(request + url_length, "\r\n\0", 3);
    if (BIO_puts(bio_ssl, request) == -1) {
        err = ERR_PUT_REQUEST;
        goto free_up_to_bio;
    }

    browser_set_status(LOADING_RECEIVING_HEADER);

    // Length: <STATUS><SPACE><META><CR><LF><NUL>
    char gemini_header[2 + 1 + 1024 + 1 + 1 + 1] = {0};
    int header_bytes = BIO_gets(bio_buffered, gemini_header, sizeof(gemini_header));
    if (header_bytes == -1) {
        err = ERR_GET_HEADER;
        goto free_up_to_bio;
    }

    struct gemini_response res = {0};
    res.status = (gemini_header[0] - '0') * 10 + (gemini_header[1] - '0');
    if (!is_valid_gemini_header(gemini_header, header_bytes)) {
        err = ERR_MALFORMED_HEADER;
        goto free_up_to_bio;
    }
    int meta_length = header_bytes - 2 - 1 - 2;
    char *meta = SDL_malloc(meta_length + 1);
    if (meta == NULL) {
        err = ERR_OUT_OF_MEMORY;
        goto free_up_to_bio;
    }
    SDL_memcpy(meta, gemini_header + 3, meta_length);
    meta[meta_length] = '\0';
    res.meta.meta = meta;

    browser_set_status(LOADING_RECEIVING_BODY);

    // Only input (2X) responses have a body.
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
                    SDL_free(prev_body);
                    SDL_free(meta);
                    err = ERR_OUT_OF_MEMORY;
                    goto free_up_to_bio;
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
            SDL_free(body);
            SDL_free(meta);
            err = ERR_GET_BODY;
            goto free_up_to_bio;
        }
    } else {
        res.body = NULL;
    }

    // res.body and res.meta.meta are allocated in this function, but outlive it.
    // They are free with gemini_response_free();
    *result = res;

free_up_to_bio:
    BIO_free(bio_buffered);
    BIO_free(bio_ssl);
free_up_to_ssl:
    SSL_shutdown(ssl); // Result ignored on purpose
    SSL_free(ssl);
    socket_shutdown(socket_fd);
free_up_to_url:
    SDL_free(resource);
    SDL_free(port);
    SDL_free(host);

    return err;
}
