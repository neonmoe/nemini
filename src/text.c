// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

// This file contains text rendering functionality based on
// stb_truetype.h and the Atkinsons Hyperlegible & Adobe Source Code
// Pro fonts.
//
// Pro: this should work *everywhere*, there's nothing system-specific
// here.
//
// Con: the fonts take space from the executable, and system-specific
// font rendering libraries would probably do a better job at
// rendering the text, especially when it comes to non-Latin scripts.
//
// But hey, pretty good text rendering for a small amount of code.

#include <SDL.h>
#include "stretchy_buffer.h"

#include "error.h"
#include "str.h"
#include "ctx.h"

#include "font_atkinson.ttf.h"
#include "font_mono.ttf.h"

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


static stbtt_fontinfo default_font, mono_font;
enum nemini_error text_renderer_init(void) {
    int init_status = stbtt_InitFont(&default_font, atkinson_hyperlegible_font, 0);
    if (init_status == 0) { return ERR_STBTT_INIT_FAIL; }
    init_status = stbtt_InitFont(&mono_font, source_code_pro_font, 0);
    if (init_status == 0) { return ERR_STBTT_INIT_FAIL; }
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
    struct nemini_string link; // Empty for all lines except links.
    enum line_type type;
};

// Returns the substring of string found after skipping whitespace
// after index "offset". I.e. for the string "#   foo" with offset 1
// would return "foo".
static struct nemini_string get_after_whitespace(struct nemini_string string, int offset) {
    int result_offset = offset;
    for (unsigned int i = offset; i < string.length; i++) {
        char c = string.ptr[i];
        if (c != ' ' && c != '\t') {
            result_offset = i;
            break;
        }
    }
    return nemini_substring(string, result_offset, string.length - result_offset);
}

// Returns the index of the first whitespace character in the given
// string. If there's no whitespace, returns -1.
static int find_whitespace_index(struct nemini_string string) {
    for (int i = 0; i < (int) string.length; i++) {
        char c = string.ptr[i];
        if (c == ' ' || c == '\t') {
            return i;
        }
    }
    return -1;
}

static enum nemini_error text_paragraphize(struct nemini_string string,
                                           struct text_paragraph **result) {
    struct text_paragraph *paragraphs = NULL;

    bool preformatted = false;
    unsigned int start = 0;
    for (unsigned int i = 0; i < string.length; i++) {
        if (string.ptr[i] == '\n') {
            struct text_paragraph paragraph = {0};
            paragraph.string = nemini_substring(string, start, i - start);
            paragraph.link = paragraph.string;
            paragraph.link.length = 0;

            start = i + 1;
            if (nemini_string_start_matches(paragraph.string, "=>")) {
                paragraph.type = GEMINI_LINK;
                paragraph.link = get_after_whitespace(paragraph.string, 2);
                int title_index = find_whitespace_index(paragraph.link);
                if (title_index != -1) {
                    paragraph.string = get_after_whitespace(paragraph.link,
                                                            title_index);
                } else {
                    paragraph.string = paragraph.link;
                }
            } else if (nemini_string_start_matches(paragraph.string, "```")) {
                preformatted = !preformatted;
                continue;
            } else if (nemini_string_start_matches(paragraph.string, "#")) {
                int offset;
                if (paragraph.string.ptr[1] != '#') {
                    paragraph.type = GEMINI_HEADING_BIG;
                    offset = 2;
                } else if (paragraph.string.ptr[2] != '#') {
                    paragraph.type = GEMINI_HEADING_MEDIUM;
                    offset = 3;
                } else {
                    paragraph.type = GEMINI_HEADING_SMALL;
                    offset = 4;
                }
                paragraph.string =
                    get_after_whitespace(paragraph.string, offset);
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

// Returns the unicode codepoint for the character at the i'th byte of
// the string ptr is pointing at. char_width will be set to the width
// of the character in bytes, i.e. 1-4.
static enum nemini_error get_unicode_codepoint(const char *ptr, int i,
                                               int *codepoint, int *char_width) {
    // Straight from https://encoding.spec.whatwg.org/#utf-8-decoder
    unsigned char lower_boundary = 0x80;
    unsigned char upper_boundary = 0xBF;
    int bytes_needed = 0;
    int bytes_seen = 0;
    int codepoint_so_far = 0;
    for (unsigned char byte = ptr[i]; byte != '\0'; byte = ptr[++i]) {
        if (bytes_needed == 0) {
            if (byte <= 0x7F) {
                *char_width = 1;
                *codepoint = byte;
                return ERR_NONE;
            } else if (byte >= 0xC2 && byte <= 0xDF) {
                bytes_needed = 1;
                codepoint_so_far = byte & 0x1F;
            } else if (byte >= 0xE0 && byte <= 0xEF) {
                if (byte == 0xE0) { lower_boundary = 0xA0; }
                if (byte == 0xED) { upper_boundary = 0x9F; }
                bytes_needed = 2;
                codepoint_so_far = byte & 0xF;
            } else if (byte >= 0xF0 && byte <= 0xF4) {
                if (byte == 0xF0) { lower_boundary = 0x90; }
                if (byte == 0xF4) { upper_boundary = 0x8F; }
                bytes_needed = 3;
                codepoint_so_far = byte & 0x7;
            } else {
                return ERR_NOT_UTF8;
            }
            continue;
        }
        if (byte < lower_boundary || byte > upper_boundary) {
            return ERR_NOT_UTF8;
        }
        lower_boundary = 0x80;
        upper_boundary = 0xBF;
        codepoint_so_far = (codepoint_so_far << 6) | (byte & 0x3F);
        bytes_seen++;
        if (bytes_seen == bytes_needed) {
            *char_width = bytes_needed + 1;
            *codepoint = codepoint_so_far;
            return ERR_NONE;
        }
    }
    return ERR_NOT_UTF8;
}

enum nemini_error text_render(SDL_Surface **result, const char *text,
                              int width, float scale) {
    struct nemini_string string;
    enum nemini_error stringify_status = nemini_string_from(text, &string);
    if (stringify_status != ERR_NONE) {
        return stringify_status;
    }

    struct text_paragraph *paragraphs = NULL;
    enum nemini_error paragraphize_status;
    paragraphize_status = text_paragraphize(string, &paragraphs);
    if (paragraphize_status != ERR_NONE) {
        return paragraphize_status;
    }

    float sf = stbtt_ScaleForMappingEmToPixels(&default_font, 16 * scale);
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&default_font, &ascent, &descent, &line_gap);
    float line_skip = sf * (ascent - descent + line_gap);
    float y_cursor = sf * ascent;

    int dash_x0, dash_x1, dash_y0, dash_y1;
    stbtt_GetCodepointBox(&default_font, '-',
                          &dash_x0, &dash_y0, &dash_x1, &dash_y1);
    float breaking_margin = sf * (dash_x1 - dash_x0);

    // The rightwards double arrow is used to prefix links.
    int link_arrow_glyph_index = stbtt_FindGlyphIndex(&mono_font, 0x21D2);
    SDL_Log("Index: %d", link_arrow_glyph_index);

    unsigned int paragraphs_count = sb_count(paragraphs);

    // TODO: Collect glyph indices & cursor positions into a list while laying out, render after
    // (Then height can be accurately measured on the first try as well.)
    int initial_height = line_skip * paragraphs_count;
    SDL_Surface *surface = NULL;
    surface = SDL_CreateRGBSurfaceWithFormat(0, width, initial_height,
                                             32, SDL_PIXELFORMAT_RGBA32);
    SDL_LockSurface(surface);

    for (unsigned int par_i = 0; par_i < paragraphs_count; par_i++) {
        struct text_paragraph paragraph = paragraphs[par_i];

        unsigned int length = paragraph.string.length;
        unsigned int good_breaking_index = 0;
        unsigned int ok_breaking_index = 0;
        unsigned int bad_breaking_index = 0;
        float x_cursor = 0;

        int char_width = 1;
        for (unsigned int i = 0; i < length; i += char_width) {
            int codepoint = 0;
            enum nemini_error err;
            err = get_unicode_codepoint(paragraph.string.ptr, i,
                                        &codepoint, &char_width);
            if (err != ERR_NONE) {
                sb_free(paragraphs);
                return err;
            }

            int advance_raw, left_side_bearing_raw;
            stbtt_GetCodepointHMetrics(&default_font, codepoint,
                                       &advance_raw, &left_side_bearing_raw);
            float adv = sf * advance_raw;
            float left_side_bearing = sf * left_side_bearing_raw;
            if (SDL_ceil(x_cursor + adv + breaking_margin) < width) {
                bad_breaking_index = i;
            }

            // Relative, pixel-space coordinates for the bounding box
            // of the glyph.
            int bx0, bx1, by0, by1;
            stbtt_GetCodepointBitmapBoxSubpixel(&default_font, codepoint, sf, sf,
                                                x_cursor - (int) x_cursor,
                                                y_cursor - (int) y_cursor,
                                                &bx0, &by0, &bx1, &by1);
            SDL_Rect rect = {0};
            rect.x = (int)(left_side_bearing + x_cursor);
            rect.y = by1 + y_cursor;
            rect.w = bx1 - bx0;
            rect.h = by1 - by0;

            if (rect.x + rect.w > width) {
                unsigned int back_up_to = bad_breaking_index;
                for (; i > back_up_to; i--) {
                    // Pop glyph at i
                }
                x_cursor = 0;
                y_cursor += line_skip;
                continue;
            }

            int x_offset, y_offset, bm_width, bm_height;
            unsigned char *bitmap =
                stbtt_GetCodepointBitmapSubpixel(&default_font, sf, sf,
                                                 x_cursor - (int) x_cursor,
                                                 y_cursor - (int) y_cursor,
                                                 codepoint,
                                                 &bm_width, &bm_height,
                                                 &x_offset, &y_offset);

            Uint8 bpp = surface->format->BytesPerPixel;
            int pitch = surface->pitch;
            int r_shift = surface->format->Rshift;
            int g_shift = surface->format->Gshift;
            int b_shift = surface->format->Bshift;
            int a_shift = surface->format->Ashift;
            unsigned int base_color;
            if (paragraph.type == GEMINI_LINK) {
                base_color = (0x11 << r_shift)
                    | (0x14 << g_shift)
                    | (0x88 << b_shift);
            } else {
                base_color = (0x22 << r_shift)
                    | (0x22 << g_shift)
                    | (0x22 << b_shift);
            }

            for (int y = 0; y < bm_height; y++) {
                int f_y = y + y_cursor + y_offset;
                if (f_y < 0 || f_y >= initial_height) { continue; }

                for (int x = 0; x < bm_width; x++) {
                    int f_x = x + x_cursor + x_offset;
                    if (f_x < 0 || f_x >= width) { continue; }

                    unsigned int color = base_color
                        | bitmap[x + y * bm_width] << a_shift;
                    unsigned int idx = f_x + f_y * pitch / bpp;
                    ((unsigned int *)surface->pixels)[idx] = color;
                }
            }
            stbtt_FreeBitmap(bitmap, NULL);

            x_cursor += adv;
        }
        y_cursor += line_skip;
        x_cursor = 0;
    }
    sb_free(paragraphs);
    SDL_UnlockSurface(surface);
    *result = surface;

    return ERR_NONE;
}
