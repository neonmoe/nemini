#include "ctx.h"
#include "error.h"

// Check the first len characters from a and b, return true if they're equal.
static bool strings_are_equal(const char *a, const char *b, int len) {
    if (a == NULL || b == NULL) {
        return a == b;
    }
    for (int i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            return false;
        } else if (a[i] == '\0' /* also implies b[i] is nul */) {
            break;
        }
    }
    return true;
}

// The character is one of the "sub-delims" in RFC 3986.
static bool is_sub_delim(char c) {
    return c == '!' || c == '$' || c == '&' || c == '\'' || c == '('
        || c == ')' || c == '*' || c == '+' || c == ',' || c == ';'
        || c == '=';
}

// The character is one of the "unreserved" in RFC 3986.
static bool is_unreserved(char c) {
    return isalpha(c) || isdigit(c) || c == '-' || c == '.' || c == '_'
        || c == '~';
}

// The character could be part of the scheme.
static bool valid_scheme_char(char c, int nth_char) {
    return isalpha(c) ||
        (nth_char > 0 && (c == '.' || c == '-' || c == '+' || isdigit(c)));
}

// The character could be part of the host.
static bool valid_host_char(char c) {
    return c == '%' || is_unreserved(c) || is_sub_delim(c);
}

// The character could be part of the port.
static bool valid_port_char(char c) {
    return isdigit(c);
}

enum url_reading_stage {
    HOST_OR_SCHEME,
    HOST,
    PORT,
    RESOURCE,
};

// Parses the relevant url parts from input_url and allocates them
// inside host, port, and resource.
enum nemini_error parse_gemini_url(char *input_url, char **host, char **port,
                                   char **resource) {

    if (input_url == NULL) { return ERR_UNEXPECTED; }

    // These pointers point at the start of the section in input_url
    // that their name would imply.
    char *host_inline = NULL;
    char *port_inline = NULL;
    char *resource_inline = NULL;
    // These describe the length of the string that their name would
    // imply, not including any possible NUL at the end.
    int host_len = 0;
    int port_len = 0;
    int resource_len = 0;

    // The url is assumed to start with the host until a scheme is found.
    host_inline = input_url;

    enum url_reading_stage stage = HOST_OR_SCHEME;
    for (char *cursor = input_url; *cursor != 0; cursor++) {
        if (cursor - input_url > 1024) {
            return ERR_URL_TOO_LONG;
        }

        char c = *cursor;
        if (stage == HOST_OR_SCHEME) {
            if (valid_scheme_char(c, cursor - input_url)) {
                // Could be either, continue.
                host_len++;
            } else if (c == ':') {
                // Could split the scheme and host or host and port, lookahead.
                if (cursor[1] == '/' && cursor[2] == '/') {
                    // Definitely scheme. Check that it's a valid protocol:
                    if (!strings_are_equal(host_inline, "gemini", 6)) {
                        return ERR_UNSUPPORTED_PROTOCOL;
                    }
                    // Then advance the stage, update host section
                    // start and advance the cursor.
                    stage = HOST;
                    cursor += 2;
                    host_inline = cursor + 1;
                    host_len = 0;
                } else {
                    // Its a port, then.
                    stage = PORT;
                }
            } else if (valid_host_char(c)) {
                // Not a valid scheme char -> this is definitely the host now.
                stage = HOST;
                host_len++;
            } else {
                return ERR_MALFORMED_URL;
            }
        } else if (stage == HOST) {
            if (host_len > 0 && c == '/') {
                stage = RESOURCE;
                resource_inline = cursor;
            } else if (host_len > 1 && c == ':') {
                stage = PORT;
                port_inline = cursor + 1;
            } else if (valid_host_char(c)) {
                host_len++;
            } else {
                return ERR_MALFORMED_URL;
            }
        } else if (stage == PORT) {
            if (port_len > 0 && c == '/') {
                stage = RESOURCE;
                resource_inline = cursor;
            } else if (valid_port_char(c)) {
                port_len++;
            } else {
                return ERR_MALFORMED_URL;
            }
        } else if (stage == RESOURCE) {
            // Resources aren't passed to other system libraries, so
            // checking them isn't as important, and they're usually
            // the longest part. That's why there's no checking.
            resource_len++;
        }
    }

    if (host_inline == NULL) {
        return ERR_UNEXPECTED;
    } else {
        *host = malloc(host_len + 1);
        if (*host == NULL) { return ERR_OUT_OF_MEMORY; }
        memcpy(*host, host_inline, host_len);
        (*host)[host_len] = '\0';
    }

    if (port_inline == NULL) {
        *port = malloc(5);
        if (*port == NULL) {
            free(*host);
            return ERR_OUT_OF_MEMORY;
        }
        memcpy(*port, "1965\0", 5);
    } else {
        *port = malloc(port_len + 1);
        if (*port == NULL) {
            free(*host);
            return ERR_OUT_OF_MEMORY;
        }
        memcpy(*port, port_inline, port_len);
        (*port)[port_len] = '\0';
    }

    if (resource_inline == NULL) {
        *resource = malloc(1);
        if (*resource == NULL) {
            free(*host);
            free(*port);
            return ERR_OUT_OF_MEMORY;
        }
        memcpy(*resource, "\0", 1);
    } else {
        *resource = malloc(resource_len + 1);
        if (*resource == NULL) {
            free(*host);
            free(*port);
            return ERR_OUT_OF_MEMORY;
        }
        memcpy(*resource, resource_inline, resource_len);
        (*resource)[resource_len] = '\0';
    }

    return ERR_NONE;
}
