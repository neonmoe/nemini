// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <SDL.h>
#include "stretchy_buffer.h"

#include "browser.h"
#include "error.h"
#include "net.h"
#include "text.h"
#include "gemini.h"
#include "url.h"

static struct loaded_page *loaded_pages = NULL;
static SDL_TLSID tls_current_page;

// For the built-in status pages.
static const char *get_status_page(int status, const char *meta) {
    switch (status) {
    case 10: return meta;
    case 11: return meta;
    case 40: return "# 40 Temporary Failure\nThe server failed to respond. Reloading may help.";
    case 41: return "# 41 Server Unavailable\nThe server is not available, possibly due to overload or maintenance.";
    case 42: return "# 42 CGI Error\nA CGI process, or a similar system for generating dynamic content, died unexpectedly or timed out.";
    case 43: return "# 43 Proxy Error\nA proxy request failed because the server was unable to successfully complete a transaction with the remote host.";
    case 44: return "# 44 Slow Down\nWait a moment before reloading, rate limiting is in effect.";
    case 50: return "# 50 Permanent Failure\nThe server does not have a response for this resource.";
    case 51: return "# 51 Not Found\nThe server could not find this resource. The resource may exist in the future, though.";
    case 52: return "# 52 Gone\nThere was something here once, but no longer.";
    case 53: return "# 53 Proxy Request Refused\nThis server does not accept proxy requests.";
    case 59: return "# 59 Bad Request\nThe server could not respond because the request was malformed.";
    case 60: return "# 60 Client Certificate Required\nThe server requires a client certificate to access this resource.";
    case 61: return "# 61 Certificate Not Authorized\nThe provided certificate is not authorized to access this resource.";
    case 62: return "# 62 Certificate Not Valid\nThe provided certificate is not valid.";
    default: return "";
    }
}

void browser_set_status(enum loading_status new_status) {
    struct loaded_page *last = (struct loaded_page *)SDL_TLSGet(tls_current_page);
    if (last != NULL) {
        last->status = new_status;
    }
}

static int redraw_page(void *data) {
    SDL_TLSSet(tls_current_page, data, 0);
    struct loaded_page *page = (struct loaded_page *)data;

    const char *content;
    if (page->response.status / 10 == 2) {
        content = page->response.body;
    } else {
        content = get_status_page(page->response.status, page->response.meta.meta);
    }

    struct text_interactable interactable;
    SDL_Surface *surface = NULL;
    page->error = text_render(&surface, &interactable, content,
                              page->rendered_width, page->rendered_scale);

    if (page->error == ERR_NONE) {
        if (page->surface != NULL) { SDL_FreeSurface(page->surface); }
        page->surface = surface;

        text_interactable_free(page->interactable);
        page->interactable = interactable;

        browser_set_status(LOADING_DONE);
    }
    return 0;
}

static int load_page(void *data) {
    SDL_TLSSet(tls_current_page, data, 0);
    struct loaded_page *page = (struct loaded_page *)data;

    const char *parent_url = NULL;
    if (page->parent != NULL) {
        parent_url = page->parent->response.url;
    }
    page->error = net_request(page->load_url, parent_url, &page->response);
    while (page->response.status / 10 == 3 && page->error == ERR_NONE) {
        // TODO?: could render a log here "⮑/⮡/↳ redirected to ..."
        struct gemini_response prev_response = page->response;
        page->error = net_request(prev_response.meta.redirect_url,
                                  prev_response.url,
                                  &page->response);
        gemini_response_free(prev_response);
    }

    if (page->error == ERR_NONE) {
        const char *content;
        if (page->response.status / 10 == 2) {
            content = page->response.body;
        } else {
            content = get_status_page(page->response.status,
                                      page->response.meta.meta);
        }
        struct text_interactable interactable;
        SDL_Surface *surface = NULL;
        page->error = text_render(&surface, &interactable, content,
                                  page->rendered_width, page->rendered_scale);

        if (page->error == ERR_NONE) {
            page->surface = surface;
            page->interactable = interactable;
        }
        browser_set_status(LOADING_DONE);
    }

    return 0;
}

enum nemini_error browser_start_loading(const char *url,
                                        struct loaded_page *from,
                                        float initial_scroll,
                                        int page_width, float page_scale) {
    struct loaded_page page = {0};
    struct gemini_response response = {0};
    page.parent = from;
    page.children = NULL;
    page.error = ERR_NONE;
    page.status = LOADING_CONNECTING;
    page.load_url = url;
    page.response = response;
    page.rendered_width = page_width;
    page.rendered_scale = page_scale;
    page.rendered_scroll = initial_scroll;
    page.surface = NULL;
    page.texture = NULL;
    sb_push(loaded_pages, page);
    void *page_ptr = (void *)(&sb_last(loaded_pages));
    SDL_TLSSet(tls_current_page, page_ptr, 0);

    if (from != NULL) {
        sb_push(from->children, page_ptr);
    }

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

struct loaded_page *browser_get_root(struct loaded_page *page) {
    // TODO: Figure out why the next line is segfaulting
    if (page == NULL || page->parent == NULL) {
        return page;
    } else {
        return browser_get_root(page->parent);
    }
}

void browser_init(void) {
    tls_current_page = SDL_TLSCreate();
}

void browser_free(void) {
    int page_count = sb_count(loaded_pages);
    for (int i = 0; i < page_count; i++) {
        sb_free(loaded_pages[i].children);
        SDL_FreeSurface(loaded_pages[i].surface);
        SDL_DestroyTexture(loaded_pages[i].texture);
        gemini_response_free(loaded_pages[i].response);
        text_interactable_free(loaded_pages[i].interactable);
    }
    sb_free(loaded_pages);
}
