// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NEMINI_BROWSER_H
#define NEMINI_BROWSER_H

#include <SDL.h>
#include "error.h"
#include "gemini.h"
#include "text.h"

enum loading_status {
    LOADING_CONNECTING,
    LOADING_ESTABLISHING_TLS,
    LOADING_SENDING_REQUEST,
    LOADING_RECEIVING_HEADER,
    LOADING_RECEIVING_BODY,
    LOADING_LAYOUT,
    LOADING_RASTERIZING,
    LOADING_DONE,
};

struct loaded_page {
    struct loaded_page *parent;
    struct loaded_page **children;
    enum nemini_error error;
    enum loading_status status;
    const char *load_url;
    struct gemini_response response;
    int rendered_width;
    float rendered_scale;
    float rendered_scroll;
    struct text_interactable interactable;

    // The surface pointer works as a kind of "queue" for the texture
    // pointer: when this page is rendered, the surface is checked: if
    // non-null, it is uploaded to the texture and freed.
    SDL_Surface *surface;
    SDL_Texture *texture;
};

void browser_set_status(enum loading_status new_status);
enum nemini_error browser_start_loading(const char *url,
                                        struct loaded_page *from,
                                        float initial_scroll,
                                        int page_width, float page_scale);
void browser_redraw_page(struct loaded_page *page, int page_width,
                         float page_scale);
struct loaded_page *browser_get_page(void);
void browser_init(void);
void browser_free(void);

#endif
