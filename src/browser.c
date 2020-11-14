// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <SDL.h>
#include "stretchy_buffer.h"

#include "browser.h"
#include "error.h"
#include "net.h"
#include "text.h"
#include "gemini.h"

static struct loaded_page *loaded_pages = NULL;
static SDL_TLSID tls_current_page;

void browser_set_status(enum loading_status new_status) {
    struct loaded_page *last = (struct loaded_page *)SDL_TLSGet(tls_current_page);
    if (last != NULL) {
        last->status = new_status;
    }
}

static int redraw_page(void *data) {
    SDL_TLSSet(tls_current_page, data, 0);
    struct loaded_page *page = (struct loaded_page *)data;

    SDL_Surface *surface = NULL;
    page->error = text_render(&surface, page->response.body,
                              page->rendered_width, page->rendered_scale);

    if (page->error == ERR_NONE) {
        page->surface = surface;
        browser_set_status(LOADING_DONE);
    }
    return 0;
}

static int load_page(void *data) {
    SDL_TLSSet(tls_current_page, data, 0);
    struct loaded_page *page = (struct loaded_page *)data;

    page->error = net_request(page->url, &page->response);

    if (page->error == ERR_NONE) {
        SDL_Surface *surface = NULL;
        page->error = text_render(&surface, page->response.body,
                                  page->rendered_width, page->rendered_scale);

        if (page->error == ERR_NONE) {
            if (page->surface == NULL) {
                SDL_FreeSurface(page->surface);
            }
            page->surface = surface;
            browser_set_status(LOADING_DONE);
        }
    }

    return 0;
}

enum nemini_error browser_start_loading(const char *url, int page_width,
                                        float page_scale) {
    struct loaded_page page = {0};
    page.parent = NULL;
    page.children = NULL;
    page.error = ERR_NONE;
    page.status = LOADING_CONNECTING;
    page.url = url;
    page.rendered_width = page_width;
    page.rendered_scale = page_scale;
    page.surface = NULL;
    page.texture = NULL;
    sb_push(loaded_pages, page);
    void *page_ptr = (void *)(&sb_last(loaded_pages));
    SDL_TLSSet(tls_current_page, page_ptr, 0);

    SDL_Thread *thread = SDL_CreateThread(load_page, "GeminiLoader", page_ptr);
    if (thread != NULL) {
        SDL_DetachThread(thread);
        return ERR_NONE;
    } else {
        return ERR_SDL;
    }
}

void browser_redraw_page(struct loaded_page *page, int page_width,
                         float page_scale) {
    if (page->status == LOADING_DONE) {
        page->rendered_width = page_width;
        page->rendered_scale = page_scale;
        SDL_Thread *thread = SDL_CreateThread(redraw_page, "PageRedraw", page);
        if (thread != NULL) {
            SDL_DetachThread(thread);
        }
    }
}

struct loaded_page *browser_get_page(void) {
    return (struct loaded_page *)SDL_TLSGet(tls_current_page);
}

void browser_init(void) {
    tls_current_page = SDL_TLSCreate();
}

void browser_free(void) {
    int page_count = sb_count(loaded_pages);
    for (int i = 0; i < page_count; i++) {
        SDL_FreeSurface(loaded_pages[i].surface);
        SDL_DestroyTexture(loaded_pages[i].texture);
        gemini_response_free(loaded_pages[i].response);
    }
    sb_free(loaded_pages);
}
