#ifndef NEMINI_ERROR_H
#define NEMINI_ERROR_H

#include <SDL.h>

// The singular error type included in this program, so that every
// error code is distinct. This also allows passing the error onwards
// without losing meaning.

enum nemini_error {
    ERR_NONE,
    ERR_UNEXPECTED,
    ERR_OUT_OF_MEMORY,

    // The following are generated by url.c:
    ERR_UNSUPPORTED_PROTOCOL,
    ERR_MALFORMED_URL,
    ERR_URL_TOO_LONG,

    // The following are generated by socket.c:
    ERR_DNS_RESOLUTION,
    ERR_CONNECT,

    // The following are generated by net.c:
    ERR_SSL_CTX_INIT,
    ERR_SSL_TLS_MIN_FAILED,
    ERR_SSL_CONNECT,
    ERR_SSL_CERT_MISSING,
    ERR_SSL_CERT_VERIFY,
    ERR_PUT_REQUEST,
    ERR_GET_HEADER,
    ERR_MALFORMED_HEADER,
    ERR_GET_BODY,
};

const char *get_nemini_err_str(enum nemini_error err);

#endif
