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
#include "text.h"
#include "browser.h"

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
    for (unsigned int i = 0; i <= string.length; i++) {
        if (string.ptr[i] == '\n' || i == string.length) {
            struct text_paragraph paragraph = {0};
            paragraph.string = nemini_substring(string, start, i - start);
            paragraph.link = paragraph.string;
            paragraph.link.length = 0;

            start = i + 1;
            if (nemini_string_start_matches(paragraph.string, "```")) {
                preformatted = !preformatted;
                continue;
            } else if (preformatted) {
                paragraph.type = GEMINI_PREFORMATTED;
            } else if (nemini_string_start_matches(paragraph.string, "=>")) {
                paragraph.type = GEMINI_LINK;
                paragraph.link = get_after_whitespace(paragraph.string, 2);
                int title_index = find_whitespace_index(paragraph.link);
                if (title_index != -1) {
                    paragraph.string = get_after_whitespace(paragraph.link,
                                                            title_index);
                    paragraph.link = nemini_substring(paragraph.link, 0,
                                                      title_index);
                } else {
                    paragraph.string = paragraph.link;
                }
            } else if (nemini_string_start_matches(paragraph.string, "#")) {
                int offset;
                if (paragraph.string.ptr[1] != '#') {
                    paragraph.type = GEMINI_HEADING_BIG;
                    offset = 1;
                } else if (paragraph.string.ptr[2] != '#') {
                    paragraph.type = GEMINI_HEADING_MEDIUM;
                    offset = 2;
                } else {
                    paragraph.type = GEMINI_HEADING_SMALL;
                    offset = 3;
                }
                paragraph.string =
                    get_after_whitespace(paragraph.string, offset);
            } else if (nemini_string_start_matches(paragraph.string, "* ")) {
                paragraph.type = GEMINI_UNORDERED_LIST;
            } else if (nemini_string_start_matches(paragraph.string, ">")) {
                paragraph.type = GEMINI_QUOTE;
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
                if (char_width != NULL) { *char_width = 1; }
                if (codepoint != NULL) { *codepoint = byte; }
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
            if (char_width != NULL) { *char_width = bytes_needed + 1; }
            if (codepoint != NULL) { *codepoint = codepoint_so_far; }
            return ERR_NONE;
        }
    }
    return ERR_NOT_UTF8;
}

static bool codepoint_is_breaking(int codepoint) {
    return codepoint == 0x20 /* space */
        || codepoint == 0x2D /* hyphen-minus */;
}

static float size_by_line_type(enum line_type type) {
    switch (type) {
    case GEMINI_HEADING_BIG: return 28;
    case GEMINI_HEADING_MEDIUM: return 24;
    case GEMINI_HEADING_SMALL: return 20;
    default: return 16;
    }
}

struct glyph_blueprint {
    struct stbtt_fontinfo *font;
    float sf;
    int glyph_index;
    enum line_type line_type;
    float x_cursor;
    float y_cursor;
};

static SDL_Surface *render_glyphs(struct glyph_blueprint *glyphs,
                                  int scale, int width, int height) {
    SDL_Surface *surface = NULL;
    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                             SDL_PIXELFORMAT_RGBA32);
    SDL_LockSurface(surface);

    int underline_start_x = -1;
    unsigned int glyphs_count = sb_count(glyphs);
    for (unsigned int i = 0; i < glyphs_count; i++) {
        struct glyph_blueprint glyph = glyphs[i];

        float x_cursor_fract = glyph.x_cursor - (int) glyph.x_cursor;
        float y_cursor_fract = glyph.y_cursor - (int) glyph.y_cursor;
        int x_offset, y_offset, bm_width, bm_height;
        unsigned char *bitmap =
            stbtt_GetGlyphBitmapSubpixel(glyph.font, glyph.sf, glyph.sf,
                                         x_cursor_fract,
                                         y_cursor_fract,
                                         glyph.glyph_index,
                                         &bm_width, &bm_height,
                                         &x_offset, &y_offset);

        Uint8 bpp = surface->format->BytesPerPixel;
        int pitch = surface->pitch / bpp; // pitch in pixels, not bytes
        int r_shift = surface->format->Rshift;
        int g_shift = surface->format->Gshift;
        int b_shift = surface->format->Bshift;
        int a_shift = surface->format->Ashift;
        unsigned int *surf_pixels = (unsigned int *)surface->pixels;

        unsigned int base_color;
        if (glyph.line_type == GEMINI_LINK) {
            base_color = (0x11 << r_shift)
                | (0x14 << g_shift)
                | (0x88 << b_shift);
        } else {
            base_color = (0x22 << r_shift)
                | (0x22 << g_shift)
                | (0x22 << b_shift);
        }

        for (int y = 0; y < bm_height; y++) {
            int f_y = y + glyph.y_cursor + y_offset;
            if (f_y < 0 || f_y >= height) { continue; }

            for (int x = 0; x < bm_width; x++) {
                int f_x = x + glyph.x_cursor + x_offset;
                if (f_x < 0 || f_x >= width) { continue; }

                unsigned int idx = f_x + f_y * pitch;
                unsigned int bm_idx = x + y * bm_width;
                int alpha = (int)((surf_pixels[idx] >> a_shift) & 0xFF);
                int new_alpha = (int)bitmap[bm_idx] + alpha;
                unsigned char final_alpha = SDL_min(0xFF, new_alpha);
                surf_pixels[idx] = base_color | (final_alpha << a_shift);
            }
        }

        if (glyph.line_type == GEMINI_LINK) {
            if (underline_start_x == -1) {
                underline_start_x = glyph.x_cursor;
            } else if (i == glyphs_count - 1
                       || glyphs[i + 1].y_cursor != glyph.y_cursor) {
                int start_x = SDL_max(0, underline_start_x);
                int end_x =
                    SDL_min(width, glyph.x_cursor + x_offset + bm_width);
                for (int offs_y = 0; offs_y < scale; offs_y++) {
                    int underline_y = glyph.y_cursor + 2 * scale + offs_y;
                    for (int x = start_x; x < end_x; x++) {
                        unsigned int idx = x + underline_y * pitch;
                        unsigned char prev_a = x == 0 ? 0 :
                            ((surf_pixels[idx - 1] >> a_shift) & 0xFF);
                        unsigned char next_a = x == width - 1 ? 0 :
                            ((surf_pixels[idx + 1] >> a_shift) & 0xFF);
                        unsigned char link_a = 0x88;
                        if ((prev_a == 0 || prev_a == link_a) && next_a == 0) {
                            surf_pixels[idx] = base_color
                                | (link_a << a_shift);
                        }
                    }
                }
                underline_start_x = -1;
            }
        }

        stbtt_FreeBitmap(bitmap, NULL);
    }

    SDL_UnlockSurface(surface);
    return surface;
}

int text_line_height(float scale) {
    float font_size = size_by_line_type(GEMINI_TEXT);
    float sf = stbtt_ScaleForMappingEmToPixels(&default_font,
                                               font_size * scale);
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&default_font, &ascent, &descent, &line_gap);
    return sf * (ascent - descent + line_gap);
}

enum nemini_error text_render(SDL_Surface **result,
                              struct text_interactable *result_interactable,
                              const char *text, int width, float scale) {
    browser_set_status(LOADING_LAYOUT);

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

    // The rightwards double arrow is used to prefix links.
    int link_arrow_glyph_index = stbtt_FindGlyphIndex(&mono_font, 0x21D2);
    int link_arrow_advance_raw, link_arrow_lsb_raw;
    stbtt_GetGlyphHMetrics(&mono_font, link_arrow_glyph_index,
                           &link_arrow_advance_raw,
                           &link_arrow_lsb_raw);

    struct text_interactable interactable = {0};
    interactable.links = NULL;

    float y_cursor = 0;
    struct glyph_blueprint *glyphs = NULL;
    unsigned int paragraphs_count = sb_count(paragraphs);

    for (unsigned int par_i = 0; par_i < paragraphs_count; par_i++) {
        struct text_paragraph paragraph = paragraphs[par_i];
        unsigned int length = paragraph.string.length;

        stbtt_fontinfo *paragraph_font;
        if (paragraph.type == GEMINI_PREFORMATTED) {
            paragraph_font = &mono_font;
        } else {
            paragraph_font = &default_font;
        }

        float font_size = size_by_line_type(paragraph.type);
        float paragraph_sf =
            stbtt_ScaleForMappingEmToPixels(paragraph_font, font_size * scale);

        int ascent, descent, line_gap;
        stbtt_GetFontVMetrics(paragraph_font, &ascent, &descent, &line_gap);

        int dash_glyph_index = stbtt_FindGlyphIndex(paragraph_font,
                                                    0x2D /* - */);
        int dash_x0, dash_x1, dy0, dy1;
        stbtt_GetGlyphBox(paragraph_font, dash_glyph_index,
                          &dash_x0, &dy0, &dash_x1, &dy1);
        float breaking_margin = paragraph_sf * (dash_x1 - dash_x0);

        unsigned int good_breaking_index = 0;
        unsigned int bad_breaking_index = 1;
        unsigned int bad_break_margin = (int)(width * 0.8);

        float line_start_y = y_cursor;
        float line_end_x = 0;
        float x_cursor = 0;
        y_cursor += paragraph_sf * ascent;

        if (paragraph.type == GEMINI_LINK) {
            struct glyph_blueprint new_glyph = {0};
            new_glyph.font = &mono_font;
            new_glyph.sf = paragraph_sf;
            new_glyph.glyph_index = link_arrow_glyph_index;
            new_glyph.line_type = GEMINI_LINK;
            new_glyph.x_cursor = x_cursor;
            new_glyph.y_cursor = y_cursor;
            sb_push(glyphs, new_glyph);
            x_cursor += paragraph_sf * link_arrow_advance_raw * 2;
        }

        int char_width = 1;
        for (unsigned int i = 0; i < length; i += char_width) {
            int codepoint = 0;
            enum nemini_error err;
            err = get_unicode_codepoint(paragraph.string.ptr, i,
                                        &codepoint, &char_width);
            if (err != ERR_NONE) {
                sb_free(glyphs);
                sb_free(paragraphs);
                text_interactable_free(interactable);
                return err;
            }

            int next_codepoint = -1;
            get_unicode_codepoint(paragraph.string.ptr, i,
                                  &next_codepoint, NULL);

            int glyph_index = stbtt_FindGlyphIndex(paragraph_font, codepoint);
            stbtt_fontinfo *font = paragraph_font;
            if (glyph_index == 0 && font == &default_font) {
                // Fall back to the mono font for individual not-found glyphs.
                // (This is especially useful for the Atkinson + SourceCodePro
                //  combination, since SourceCodePro has a lot more symbols.)
                glyph_index = stbtt_FindGlyphIndex(&mono_font, codepoint);
                if (glyph_index != 0) {
                    font = &mono_font;
                }
            }

            int next_glyph_index = stbtt_FindGlyphIndex(font, next_codepoint);
            float sf = stbtt_ScaleForMappingEmToPixels(font,
                                                       font_size * scale);

            int advance_raw, lsb_raw;
            stbtt_GetGlyphHMetrics(font, glyph_index, &advance_raw, &lsb_raw);
            int kerning_adv_raw = stbtt_GetGlyphKernAdvance(font, glyph_index,
                                                            next_glyph_index);

            float adv = sf * (advance_raw + kerning_adv_raw);
            if (SDL_ceil(x_cursor + adv + breaking_margin) < width) {
                if (x_cursor + adv > bad_break_margin
                    && codepoint_is_breaking(codepoint)) {
                    good_breaking_index = i;
                } else {
                    bad_breaking_index = i + char_width;
                }
            }

            // Relative, pixel-space coordinates for the bounding box
            // of the glyph.
            int bx0, bx1, by0, by1;
            stbtt_GetGlyphBitmapBoxSubpixel(font, glyph_index, sf, sf,
                                            x_cursor - (int) x_cursor,
                                            y_cursor - (int) y_cursor,
                                            &bx0, &by0, &bx1, &by1);
            if (x_cursor + bx1 > width) {
                unsigned int back_up_to;
                if (good_breaking_index > 0) {
                    back_up_to = good_breaking_index;
                    good_breaking_index = 0;
                } else {
                    back_up_to = bad_breaking_index;
                    // Bad breaking points need a hyphen to indicate
                    // the forced break. Obviously, it isn't properly
                    // hyphenated, but still.
                    struct glyph_blueprint new_glyph = {0};
                    new_glyph.font = font;
                    new_glyph.sf = sf;
                    new_glyph.glyph_index = dash_glyph_index;
                    new_glyph.line_type = paragraph.type;
                    new_glyph.x_cursor = x_cursor;
                    new_glyph.y_cursor = y_cursor;
                    sb_push(glyphs, new_glyph);
                }

                i--;
                for (; i > back_up_to; i--) {
                    sb_pop(glyphs);
                }
                // After moving the index, recalculate the char width
                get_unicode_codepoint(paragraph.string.ptr, i,
                                      NULL, &char_width);

                x_cursor = 0;
                y_cursor += sf * (ascent - descent + line_gap);
            } else {
                struct glyph_blueprint new_glyph = {0};
                new_glyph.font = font;
                new_glyph.sf = sf;
                new_glyph.glyph_index = glyph_index;
                new_glyph.line_type = paragraph.type;
                new_glyph.x_cursor = x_cursor;
                new_glyph.y_cursor = y_cursor;
                sb_push(glyphs, new_glyph);

                x_cursor += adv;
                line_end_x = SDL_max(line_end_x, x_cursor);
            }
        }
        y_cursor += paragraph_sf * (-descent + line_gap);
        x_cursor = 0;

        if (paragraph.type == GEMINI_LINK) {
            struct link_box link = {0};
            link.rect.x = 0;
            link.rect.y = line_start_y;
            link.rect.w = line_end_x;
            link.rect.h = y_cursor - line_start_y;
            link.link = SDL_malloc(paragraph.link.length + 1);
            SDL_memcpy(link.link, paragraph.link.ptr, paragraph.link.length);
            link.link[paragraph.link.length] = '\0';
            sb_push(interactable.links, link);
        }
    }
    sb_free(paragraphs);

    browser_set_status(LOADING_RASTERIZING);

    int height = (int)y_cursor;
    SDL_Surface *surface = render_glyphs(glyphs, (int)scale, width, height);
    sb_free(glyphs);

    *result_interactable = interactable;
    *result = surface;

    return ERR_NONE;
}

struct cached_texture {
    SDL_Texture *texture;
    char *text;
    float scale;
};

static struct cached_texture *cache = NULL;

SDL_Texture *text_cached_render(SDL_Renderer *renderer,
                                const char *text, float scale) {
    int cache_length = sb_count(cache);
    for (int i = 0; i < cache_length; i++) {
        struct cached_texture tex = cache[i];
        if (SDL_strcmp(text, tex.text) == 0 && tex.scale == scale) {
            return tex.texture;
        }
    }

    float y_cursor = 0;
    struct glyph_blueprint *glyphs = NULL;

    stbtt_fontinfo *font = &default_font;
    enum line_type type = GEMINI_HEADING_MEDIUM;

    float font_size = size_by_line_type(type);
    float paragraph_sf =
        stbtt_ScaleForMappingEmToPixels(font, font_size * scale);

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(font, &ascent, &descent, &line_gap);

    float x_cursor = 0;
    y_cursor += paragraph_sf * ascent;

    int char_width = 1;
    for (unsigned int i = 0; text[i] != '\0'; i += char_width) {
        int codepoint = 0;
        enum nemini_error err;
        err = get_unicode_codepoint(text, i, &codepoint, &char_width);
        if (err != ERR_NONE) {
            sb_free(glyphs);
            return NULL;
        }

        int next_codepoint = -1;
        get_unicode_codepoint(text, i, &next_codepoint, NULL);
        int glyph_index = stbtt_FindGlyphIndex(font, codepoint);
        int next_glyph_index = stbtt_FindGlyphIndex(font, next_codepoint);
        float sf = stbtt_ScaleForMappingEmToPixels(font, font_size * scale);

        int advance_raw, lsb_raw;
        stbtt_GetGlyphHMetrics(font, glyph_index, &advance_raw, &lsb_raw);
        int kerning_adv_raw = stbtt_GetGlyphKernAdvance(font, glyph_index,
                                                        next_glyph_index);
        float adv = sf * (advance_raw + kerning_adv_raw);

        struct glyph_blueprint new_glyph = {0};
        new_glyph.font = font;
        new_glyph.sf = sf;
        new_glyph.glyph_index = glyph_index;
        new_glyph.line_type = type;
        new_glyph.x_cursor = x_cursor;
        new_glyph.y_cursor = y_cursor;
        sb_push(glyphs, new_glyph);

        x_cursor += adv;
    }

    y_cursor -= paragraph_sf * descent;

    int width = (int)x_cursor;
    int height = (int)y_cursor;
    SDL_Surface *surface = render_glyphs(glyphs, (int)scale, width, height);
    sb_free(glyphs);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (texture != NULL) {
        int len = SDL_strlen(text);
        char *cloned_text = SDL_malloc(len + 1);
        SDL_memcpy(cloned_text, text, len);
        cloned_text[len] = '\0';
        struct cached_texture tex = {0};
        tex.texture = texture;
        tex.text = cloned_text;
        tex.scale = scale;
        sb_push(cache, tex);
    }

    return texture;
}

void text_renderer_free(void) {
    int cache_length = sb_count(cache);
    for (int i = 0; i < cache_length; i++) {
        struct cached_texture tex = cache[i];
        SDL_free(tex.text);
        SDL_DestroyTexture(tex.texture);
    }
}

void text_interactable_free(struct text_interactable interactable) {
    int link_count = sb_count(interactable.links);
    for (int i = 0; i < link_count; i++) {
        SDL_free(interactable.links[i].link);
    }
    sb_free(interactable.links);
}
