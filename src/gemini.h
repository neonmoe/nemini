#ifndef NEMINI_GEMINI_H
#define NEMINI_GEMINI_H

#include "ctx.h"

struct gemini_response {
    // Gemini status code.
    int status;

    // Pointer to the <META> string.
    union {
        // Generic meta string name.
        char *meta;
        // For input (1X) responses
        char *input_prompt;
        // For success (2X) responses
        char *mime_type;
        // For redirect (3X) responses
        char *redirect_url;
        // For temporary and permanent failure (4X, 5X) responses
        char *failure_reason;
        // For client certificate required (6X) responses
        char *client_cert_error;
    } meta;

    // Pointer to the body, or NULL if there is no body.
    char *body;
};

bool is_valid_gemini_header(const char* header, int header_length);

void gemini_response_free(struct gemini_response);

#endif
