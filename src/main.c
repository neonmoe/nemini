// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <SDL.h>

#include "ctx.h"
#include "socket.h"
#include "net.h"
#include "error.h"
#include "gemini.h"
#include "text.h"
#include "browser.h"

int get_refresh_rate(SDL_Window *);
bool get_scale(SDL_Window *, SDL_Renderer *, float *, float *);

int main(int argc, char **argv) {
    int sockets_status = sockets_init();
    if (sockets_status != ERR_NONE) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't initialize networking: %s",
                     get_nemini_err_str(sockets_status));
        return 1;
    }

    int net_status = net_init();
    if (net_status != ERR_NONE) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't initialize TLS: %s",
                     get_nemini_err_str(net_status));
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't initialize SDL: %s",
                     SDL_GetError());
        return 1;
    }

    SDL_Window *window;
    SDL_Renderer *renderer;
    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (SDL_CreateWindowAndRenderer(640, 480, flags, &window, &renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't create window and renderer: %s",
                     SDL_GetError());
        return 1;
    }

    int text_renderer_status = text_renderer_init();
    if (text_renderer_status != ERR_NONE) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't init the text rendering system: %s",
                     get_nemini_err_str(text_renderer_status));
        return 1;
    }

    SDL_SetWindowTitle(window, "Nemini");

    browser_set_status(LOADING_CONNECTING);
    if (argc >= 2) {
        browser_start_loading(argv[1]);
    } else {
        SDL_Log("Usage: %s <url>", argv[0]);
        return 0;
    }

    bool running = true;
    int refresh_rate = 60;
    while (running) {
        Uint32 frame_start_ms = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }
        }
        if (!running) {
            break;
        }

        float scale_x, scale_y;
        if (get_scale(window, renderer, &scale_x, &scale_y)) {
            SDL_RenderSetScale(renderer, scale_x, scale_y);
        }
        int width, height;
        SDL_GetRendererOutputSize(renderer, &width, &height);
        width /= scale_x;
        height /= scale_y;
        SDL_SetRenderDrawColor(renderer, 0xDD, 0xDD, 0xDD, 0xDD);
        SDL_RenderClear(renderer);

        enum loading_status page_status = browser_get_status();
        if (page_status == LOADING_DONE) {
            struct loaded_page *page = get_browser_page();

            if (page->texture == NULL) {
                page->texture = SDL_CreateTextureFromSurface(renderer, page->surface);
                if (page->texture != NULL) {
                    SDL_FreeSurface(page->surface);
                    page->surface = NULL;
                }
            } else {
                Uint32 t_format;
                int t_access, t_width, t_height;
                SDL_QueryTexture(page->texture, &t_format, &t_access,
                                 &t_width, &t_height);
                t_width /= scale_x;
                t_height /= scale_y;
                SDL_Rect dst_rect = {0};
                dst_rect.x = (width - t_width) / 2;
                dst_rect.y = 8;
                dst_rect.w = t_width;
                dst_rect.h = t_height;
                SDL_RenderCopy(renderer, page->texture, NULL, &dst_rect);
            }
        } else {
            SDL_SetRenderDrawColor(renderer, 0x55, 0xAA, 0x55, 0xBB);

            for (int s = 0; s < LOADING_DONE; s++) {
                SDL_Rect loading_rect = {0};
                loading_rect.x = (width - 40 * LOADING_DONE) / 2
                    + s * 40;
                loading_rect.y = (height - 32) / 2;
                loading_rect.w = 32;
                loading_rect.h = 32;
                if (s < (int)page_status) {
                    SDL_RenderFillRect(renderer, &loading_rect);
                } else {
                    SDL_RenderDrawRect(renderer, &loading_rect);
                }
            }
        }

        SDL_RenderPresent(renderer);

        // Temporary fps displayer until the text rendering system is
        // up and running.
        static int frame_counter = 0;
        static long last_time = 0;
        frame_counter++;
        long now = SDL_GetTicks();
        if (now - last_time >= 1000) {
            last_time = now;
            char buf[1024];
            SDL_snprintf(buf, 1024, "FPS: %d (avg. frame duration: %.2f ms)",
                         frame_counter, 1000.0 / frame_counter);
            SDL_SetWindowTitle(window, buf);
            frame_counter = 0;
        }

        int new_refresh_rate = get_refresh_rate(window);
        if (new_refresh_rate > 0) {
            refresh_rate = new_refresh_rate;
        }

        Uint32 frame_duration = SDL_GetTicks() - frame_start_ms;
        Uint32 target_duration = 1000 / (Uint32) refresh_rate;
        if (frame_duration < target_duration) {
            SDL_Delay(target_duration - frame_duration);
        }
    }

    browser_free();
    text_renderer_free();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    net_free();
    sockets_free();
    SDL_Quit();
    return 0;
}

// Returns refresh rate on success, -1 on error.
int get_refresh_rate(SDL_Window *window) {
    int display_index = SDL_GetWindowDisplayIndex(window);
    if (display_index >= 0) {
        SDL_DisplayMode mode = {0};
        if (SDL_GetDisplayMode(display_index, 0, &mode) < 0) {
            return -1;
        }
        if (mode.refresh_rate != 0) {
            return mode.refresh_rate;
        }
    }
    return -1;
}

// Returns true on success, scaleX and scaleY are filled.
bool get_scale(SDL_Window *window, SDL_Renderer *renderer,
               float *scale_x, float *scale_y) {
    int logical_width, logical_height;
    SDL_GetWindowSize(window, &logical_width, &logical_height);
    int physical_width, physical_height;
    if (SDL_GetRendererOutputSize(renderer,
                                  &physical_width,
                                  &physical_height)) {
        return false;
    }
    *scale_x = (float) physical_width / (float) logical_width;
    *scale_y = (float) physical_height / (float) logical_height;
    return true;
}
