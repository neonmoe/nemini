// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <SDL.h>
#include "error.h"
#include "str.h"
#include "stretchy_buffer.h"
#include "ctx.h"

#define STBTT_ifloor(x) ((int) SDL_floor(x))
#define STBTT_iceil(x) ((int) SDL_ceil(x))
#define STBTT_sqrt(x) SDL_sqrt(x)
#define STBTT_pow(x, y) SDL_pow(x, y)
#define STBTT_fmod(x, y) SDL_fmod(x, y)
#define STBTT_cos(x) SDL_cos(x)
#define STBTT_acos(x) SDL_acos(x)
#define STBTT_fabs(x) SDL_fabs(x)
#define STBTT_malloc(x,u) ((void)(u),SDL_malloc(x))
#define STBTT_free(x,u) ((void)(u),SDL_free(x))
#define STBTT_assert(x) SDL_assert(x)
#define STBTT_strlen(x) SDL_strlen(x)
#define STBTT_memcpy SDL_memcpy
#define STBTT_memset SDL_memset
// The implementation is included as static in this file, since it a)
// shouldn't be needed anywhere else, and b) hopefully allow compilers
// to be more aggressive about cleaning up dead code, since it's all
// internal to text.c.
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "stb_truetype.h"
#pragma GCC diagnostic pop

enum nemini_error text_renderer_init(void) {
    return ERR_NONE;
}

void text_renderer_free(void) {
}

enum line_type {
    GEMINI_TEXT,
    GEMINI_LINK,
    GEMINI_PREFORMATTED,
    GEMINI_HEADING_BIG,
    GEMINI_HEADING_MEDIUM,
    GEMINI_HEADING_SMALL,
    GEMINI_UNORDERED_LIST,
    GEMINI_QUOTE,
};

// text/gemini is rendered one line at a time, with wrapping. The
// "lines" from the Gemini spec are called paragraphs in Nemini, to
// differentiate them from actual visual lines.
struct text_paragraph {
    struct nemini_string string;
    enum line_type type;
};

static enum nemini_error text_paragraphize(struct nemini_string string,
                                           struct text_paragraph **result) {
    struct text_paragraph *paragraphs = NULL;
    bool preformatted = false;
    unsigned int start = 0;
    for (unsigned i = 0; i < string.length; i++) {
        if (string.ptr[i] == '\n') {
            struct text_paragraph paragraph = {0};
            enum nemini_error err;
            err = nemini_string_substring(string, &paragraph.string,
                                          start, i - start);
            if (err != ERR_NONE) {
                sb_free(paragraphs);
                return err;
            }
            start = i + 1;
            if (nemini_string_start_matches(paragraph.string, "=>")) {
                paragraph.type = GEMINI_LINK;
            } else if (nemini_string_start_matches(paragraph.string, "```")) {
                preformatted = !preformatted;
                continue;
            } else if (nemini_string_start_matches(paragraph.string, "#")) {
                if (paragraph.string.ptr[1] != '#') {
                    paragraph.type = GEMINI_HEADING_BIG;
                } else if (paragraph.string.ptr[2] != '#') {
                    paragraph.type = GEMINI_HEADING_MEDIUM;
                } else {
                    paragraph.type = GEMINI_HEADING_SMALL;
                }
            } else if (nemini_string_start_matches(paragraph.string, "* ")) {
                paragraph.type = GEMINI_UNORDERED_LIST;
            } else if (nemini_string_start_matches(paragraph.string, ">")) {
                paragraph.type = GEMINI_QUOTE;
            } else if (preformatted) {
                paragraph.type = GEMINI_PREFORMATTED;
            } else {
                paragraph.type = GEMINI_TEXT;
            }
            sb_push(paragraphs, paragraph);
        }
    }
    *result = paragraphs;
    return ERR_NONE;
}

enum nemini_error text_render(const char *text, SDL_Texture **texture) {
    struct nemini_string string;
    enum nemini_error stringify_status = nemini_string_from(text, &string);
    if (stringify_status != ERR_NONE) {
        return stringify_status;
    }

    struct text_paragraph *paragraphs = NULL;
    enum nemini_error paragraphize_status = text_paragraphize(string, &paragraphs);
    if (paragraphize_status != ERR_NONE) {
        return paragraphize_status;
    }

    int paragraphs_count = sb_count(paragraphs);
    for (int i = 0; i < paragraphs_count; i++) {
        struct text_paragraph paragraph = paragraphs[i];
        char *cstr = SDL_malloc(paragraph.string.length + 1);
        SDL_memcpy(cstr, paragraph.string.ptr, paragraph.string.length);
        cstr[paragraph.string.length] = '\0';
        SDL_Log("Paragraph (type %d): %s", paragraph.type, cstr);
        SDL_free(cstr);
    }

    sb_free(paragraphs);
    return ERR_NONE;
}

