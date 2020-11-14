// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NEMINI_TEXT_H
#define NEMINI_TEXT_H

#include "error.h"
#include "str.h"

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
    struct nemini_string link; // Empty for all lines except links.
    enum line_type type;
};

enum nemini_error text_renderer_init(void);
void text_renderer_free(void);
enum nemini_error text_render(SDL_Surface **result,
                              const char *text, int width, float scale);
SDL_Texture *text_cached_render(SDL_Renderer *renderer,
                                const char *text, float scale);

#endif
