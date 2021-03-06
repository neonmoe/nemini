// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "error.h"

const char *get_nemini_err_str(enum nemini_error err) {
    switch (err) {
    case ERR_NONE: return "no error";
    case ERR_UNEXPECTED: return "unexpected error, this is a bug";
    case ERR_OUT_OF_MEMORY: return "out of memory";
    case ERR_SDL: return SDL_GetError();
    case ERR_NOT_UTF8: return "given text is not valid UTF-8";
    case ERR_STBTT_INIT_FAIL: return "could not load the font";
    case ERR_SRC_STR_NULL: return "input string is null";
    case ERR_SUBSTR_OFFSET_OUT_OF_BOUNDS:
        return "substring start is out of bounds";
    case ERR_SUBSTR_LENGTH_OUT_OF_BOUNDS:
        return "substring end is out of bounds";
    case ERR_UNSUPPORTED_PROTOCOL: return "unsupported protocol in url";
    case ERR_MALFORMED_URL:
        return "url is not of the form [scheme://]<host>[:port][/resource]";
    case ERR_URL_TOO_LONG:
        return "url is too long to be a gemini url (>1024 chars)";
    case ERR_WSA_STARTUP: return "winsock setup failed";
    case ERR_DNS_RESOLUTION: return "could not resolve host";
    case ERR_CONNECT: return "could not connect to host";
    case ERR_SSL_CTX_INIT: return "could not initialize libssl context";
    case ERR_SSL_TLS_MIN_FAILED:
        return "could not set minimum TLS version to 1.2";
    case ERR_SSL_CONNECT: return "could not establish a TLS connection";
    case ERR_SSL_CERT_MISSING:
        return "peer certificate is missing; this should not happen";
    case ERR_SSL_CERT_VERIFY:
        return "cert verification failed: cannot trust server";
    case ERR_PUT_REQUEST: return "io error sending request";
    case ERR_GET_HEADER: return "io error receiving response header";
    case ERR_MALFORMED_HEADER:
        return "expected gemini header, got something else";
    case ERR_GET_BODY: return "io error receiving response body";
    default: return "unknown error";
    }
}
