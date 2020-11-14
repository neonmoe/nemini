// SPDX-FileCopyrightText: 2020 Jens Pitkanen <jens@neon.moe>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <SDL.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "stretchy_buffer.h"
#pragma GCC diagnostic pop

#include "ctx.h"
#include "socket.h"
#include "net.h"
#include "error.h"
#include "gemini.h"
#include "text.h"
#include "browser.h"
#include "loading.bmp.h"

int get_refresh_rate(SDL_Window *);
bool get_scale(SDL_Window *, SDL_Renderer *, float *, float *);
int get_desired_content_width(int, float);
float lerp(float, float, float);

int main(int argc, char **argv) {
    SDL_RWops *loading_img = SDL_RWFromMem((void *)loading_image_bmp,
                                           sizeof(loading_image_bmp));
    SDL_Surface *loading_surf = SDL_LoadBMP_RW(loading_img, 0);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't initialize SDL: %s",
                     SDL_GetError());
        return 1;
    }

    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    SDL_Window *window = SDL_CreateWindow("Nemini",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          640, 480, flags);
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't create window: %s",
                     SDL_GetError());
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 320, 240);

    SDL_Surface *window_surf = SDL_GetWindowSurface(window);
    if (window_surf != NULL) {
        SDL_Rect rect = {0};
        rect.x = (window_surf->w - loading_surf->w) / 2;
        rect.y = (window_surf->h - loading_surf->h) / 2;
        rect.w = loading_surf->w;
        rect.h = loading_surf->h;
        SDL_BlitSurface(loading_surf, NULL, window_surf, &rect);
        SDL_UpdateWindowSurface(window);
    }
    SDL_FreeSurface(loading_surf);
    SDL_RWclose(loading_img);

    SDL_Renderer *renderer = SDL_GetRenderer(window);
    if (renderer == NULL) {
        renderer = SDL_CreateRenderer(window, -1, 0);
        if (renderer == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Couldn't create renderer: %s",
                         SDL_GetError());
            return 1;
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    browser_init();

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

    int text_renderer_status = text_renderer_init();
    if (text_renderer_status != ERR_NONE) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't init the text rendering system: %s",
                     get_nemini_err_str(text_renderer_status));
        return 1;
    }

    SDL_Cursor *cursor_arrow, *cursor_hand, *cursor_wait;
    cursor_arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    cursor_hand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    cursor_wait = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAITARROW);

    browser_set_status(LOADING_CONNECTING);
    if (argc >= 2) {
        float scale_x, scale_y;
        if (get_scale(window, renderer, &scale_x, &scale_y)) {
            SDL_RenderSetScale(renderer, scale_x, scale_y);
        }
        int width, height;
        SDL_GetRendererOutputSize(renderer, &width, &height);

        int content_width = get_desired_content_width(width, scale_x);
        browser_start_loading(argv[1], NULL, content_width, scale_x);
    } else {
        SDL_Log("Usage: %s <url>", argv[0]);
        return 0;
    }

    // UI state:
    float loading_bar_width = 0;
    float scroll = 0;
    int content_height = 0;

    bool mouse_held = false;
    bool mouse_clicked = false;
    SDL_Point mouse = {0};

    bool running = true;
    int refresh_rate = 60;
    while (running) {
        Uint32 frame_start_ms = SDL_GetTicks();
        mouse_clicked = false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            } else if (event.type == SDL_MOUSEMOTION) {
                mouse.x = event.motion.x;
                mouse.y = event.motion.y;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (!mouse_held) {
                    mouse_clicked = true;
                }
                mouse_held = true;
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                mouse_held = false;
            } else if (event.type == SDL_MOUSEWHEEL) {
                float scale = 0;
                get_scale(window, renderer, &scale, NULL);
                float line_height = text_line_height(scale);
                scroll += event.wheel.y * line_height * 3;
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
        int bg_r = 0xDD;
        int bg_g = 0xDD;
        int bg_b = 0xDD;
        SDL_SetRenderDrawColor(renderer, bg_r, bg_g, bg_b, 0xFF);
        SDL_RenderClear(renderer);

        struct loaded_page *page = browser_get_page();
        SDL_Cursor *current_cursor = cursor_arrow;
        if (page->status != LOADING_DONE) {
            current_cursor = cursor_wait;
        }

        scroll = SDL_min(64, SDL_max(height - content_height - 64, scroll));

        if (page->texture != NULL || page->surface != NULL) {
            if (page->surface != NULL) {
                if (page->texture != NULL) {
                    SDL_DestroyTexture(page->texture);
                }
                page->texture = SDL_CreateTextureFromSurface(renderer,
                                                             page->surface);
                if (page->texture != NULL) {
                    SDL_FreeSurface(page->surface);
                    page->surface = NULL;
                }
                scroll = page->rendered_scroll;
            }

            if (page->texture != NULL) {
                page->rendered_scroll = scroll;

                Uint32 t_format;
                int t_access, t_width, t_height;
                SDL_QueryTexture(page->texture, &t_format, &t_access,
                                 &t_width, &t_height);

                int desired_width = get_desired_content_width(width, scale_x);
                if (t_width != desired_width) {
                    browser_redraw_page(page, desired_width, scale_x);
                }

                t_width /= scale_x;
                t_height /= scale_y;
                SDL_Rect dst_rect = {0};
                dst_rect.x = (width - t_width) / 2;
                dst_rect.y = page->rendered_scroll;
                dst_rect.w = t_width;
                dst_rect.h = content_height = t_height;
                SDL_RenderCopy(renderer, page->texture, NULL, &dst_rect);

                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_MOD);
                int links_count = sb_count(page->interactable.links);
                for (int i = 0; i < links_count; i++) {
                    struct link_box link = page->interactable.links[i];
                    SDL_Rect link_rect = {0};
                    link_rect.x = link.rect.x + dst_rect.x;
                    link_rect.y = link.rect.y + dst_rect.y;
                    link_rect.w = link.rect.w;
                    link_rect.h = link.rect.h;
                    if (SDL_PointInRect(&mouse, &link_rect)) {
                        current_cursor = cursor_hand;

                        if (mouse_held) {
                            SDL_SetRenderDrawColor(renderer,
                                                   0xFF, 0xEE, 0xDD, 0xFF);
                            SDL_RenderFillRect(renderer, &link_rect);
                        } else {
                            SDL_SetRenderDrawColor(renderer,
                                                   0xEE, 0xEE, 0xFF, 0xFF);
                            SDL_RenderFillRect(renderer, &link_rect);
                        }

                        if (mouse_clicked) {
                            browser_start_loading(link.link, page,
                                                  desired_width, scale_x);
                        }
                    }
                }
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            }
        }

        if (page->status != LOADING_DONE) {
            bool is_err = page->error != ERR_NONE;
            if (is_err) {
                current_cursor = cursor_arrow;

                char str[1024];
                SDL_snprintf(str, 1024, "Error: %s",
                             get_nemini_err_str(page->error));
                SDL_Texture *err_tex;
                err_tex = text_cached_render(renderer, str, scale_x);
                Uint32 t_format;
                int t_access, t_width, t_height;
                SDL_QueryTexture(err_tex, &t_format, &t_access,
                                 &t_width, &t_height);
                t_width /= scale_x;
                t_height /= scale_y;
                SDL_Rect dst_rect = {0};
                dst_rect.x = (width - t_width) / 2;
                dst_rect.y = scroll;
                dst_rect.w = t_width;
                dst_rect.h = content_height = t_height;
                SDL_SetRenderDrawColor(renderer, bg_r, bg_g, bg_b, 0xFF);
                SDL_RenderFillRect(renderer, &dst_rect);
                SDL_RenderCopy(renderer, err_tex, NULL, &dst_rect);

                SDL_SetRenderDrawColor(renderer, 0xDD, 0x33, 0x33, 0xBB);
            } else {
                SDL_SetRenderDrawColor(renderer, 0x55, 0xAA, 0x55, 0xBB);
            }

            SDL_Rect loading_rect = {0};
            loading_rect.x = 0;
            loading_rect.y = 0;
            if (is_err) {
                loading_rect.w = width;
            } else {
                float target = width * (int)page->status / (LOADING_DONE - 1);
                loading_bar_width = lerp(loading_bar_width, target,
                                         7.5 / refresh_rate);
                loading_rect.w = loading_bar_width;
            }
            loading_rect.h = 8;
            SDL_RenderFillRect(renderer, &loading_rect);
        }

        SDL_SetCursor(current_cursor);
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
    if (scale_x != NULL) {
        *scale_x = (float) physical_width / (float) logical_width;
    }
    if (scale_y != NULL) {
        *scale_y = (float) physical_height / (float) logical_height;
    }
    return true;
}

int get_desired_content_width(int width, float scale) {
    int desired_width = (int)(width * scale)
        - ((int)(width * scale) % (int)(50 * scale)) - (int)(20 * scale);
    return SDL_min(800 * scale, SDL_max(100 * scale, desired_width));
}

float lerp(float from, float to, float a) {
    return from + (to - from) * SDL_max(SDL_min(a, 1), 0);
}
