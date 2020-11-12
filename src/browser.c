// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <SDL.h>
#include "stretchy_buffer.h"

#include "browser.h"
#include "error.h"
#include "net.h"
#include "text.h"

static enum loading_status current_status;
static struct loaded_page *loaded_pages = NULL;

void browser_set_status(enum loading_status new_status) {
    current_status = new_status;
}

enum loading_status browser_get_status(void) {
    return current_status;
}

static int load_page(void *data) {
    const char *url = (char *)data;
    SDL_Log("Starting to load page from: %s", url);
    Uint32 response_start = SDL_GetTicks();

    struct gemini_response res = {0};
    int result = net_request(url, &res);

    if (result == ERR_NONE) {
        SDL_Surface *surface = NULL;
        text_render(&surface, res.body, (int)(600 * 1), 1);
        gemini_response_free(res);

        struct loaded_page page = {0};
        page.surface = surface;
        sb_push(loaded_pages, page);

        browser_set_status(LOADING_DONE);
        SDL_Log("Finished loading after %.3f seconds.", (SDL_GetTicks() - response_start) / 1000.0);
    }
    return 0;
}

enum nemini_error browser_start_loading(const char *url) {
    SDL_Thread *thread = SDL_CreateThread(load_page, "GeminiLoader", (void *)url);
    if (thread != NULL) {
        SDL_DetachThread(thread);
        return ERR_NONE;
    } else {
        return ERR_SDL;
    }
}

struct loaded_page *get_browser_page(void) {
    return &sb_last(loaded_pages);
}

void browser_free(void) {
    int page_count = sb_count(loaded_pages);
    for (int i = 0; i < page_count; i++) {
        SDL_FreeSurface(loaded_pages[i].surface);
        SDL_DestroyTexture(loaded_pages[i].texture);
    }
}
