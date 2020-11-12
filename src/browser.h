// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NEMINI_BROWSER_H
#define NEMINI_BROWSER_H

#include <SDL.h>
#include "error.h"

struct loaded_page {
    struct loaded_page *parent;

    // When loading, surface is set. After the first render, the
    // surface is loaded into texture memory, and freed.
    // So surface == null XOR texture == null.
    SDL_Surface *surface;
    SDL_Texture *texture;
};

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

void browser_set_status(enum loading_status new_status);
enum loading_status browser_get_status(void);
enum nemini_error browser_start_loading(const char *url);
struct loaded_page *get_browser_page(void);
void browser_free(void);

#endif
