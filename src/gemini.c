#include <string.h>
#include "ctx.h"
#include "gemini.h"

// Returns true if header is a valid gemini response header. The
// header_length param should be the length of the string, not
// counting the NUL byte.
bool is_valid_gemini_header(const char *header, int header_length) {
    if (header == NULL) {
        return false;
    }
    // <STATUS><SPACE>(no meta)<CR><LF>
    int min_len = 2 + 1 + 1 + 1;
    return header_length > min_len
        && isdigit(header[0])
        && isdigit(header[1])
        && header[2] == ' '
        && header[header_length - 2] == '\r'
        && header[header_length - 1] == '\n';
}

void gemini_response_free(struct gemini_response res) {
    if (res.meta != NULL) {
        free(res.meta);
    }
    if (res.body != NULL) {
        free(res.body);
    }
}
