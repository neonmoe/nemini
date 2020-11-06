#include <SDL.h>
#include <string.h>

#include "ctx.h"
#include "socket.h"
#include "net.h"
#include "error.h"
#include "gemini.h"

int get_refresh_rate(SDL_Window *);
bool get_scale(SDL_Window *, SDL_Renderer *, float *, float *);

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't initialize SDL: %s",
                     SDL_GetError());
        return 1;
    }

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
                     "Couldn't initialize SSL: %s",
                     get_nemini_err_str(net_status));
        return 1;
    }

    struct gemini_response res = {0};
    int result = net_request("gemini://localhost/", &res);
    if (result != ERR_NONE) {
        SDL_Log("net_request() returned an error: %s", get_nemini_err_str(result));
    }

    SDL_Log("%d %s\n%s",
            res.status, res.meta.mime_type, res.body);

    gemini_response_free(res);
    net_free();
    sockets_free();
    SDL_Quit();
    return 0;

    SDL_Window *window;
    SDL_Renderer *renderer;
    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (SDL_CreateWindowAndRenderer(320, 240, flags, &window, &renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't create window and renderer: %s",
                     SDL_GetError());
        return 1;
    }

    SDL_SetWindowTitle(window, "Nemini");

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

        SDL_SetRenderDrawColor(renderer, 0x22, 0x44, 0x22, 0xFF);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);

        int new_refresh_rate = get_refresh_rate(window);
        if (new_refresh_rate > 0) {
            refresh_rate = new_refresh_rate;
        }

        Uint32 frame_duration = SDL_GetTicks() - frame_start_ms;
        Uint32 target_duration = 1000 / (Uint32) refresh_rate;
        if (frame_duration > 0 && frame_duration < target_duration) {
            SDL_Delay(target_duration - frame_duration);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

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
bool get_scale(SDL_Window *window, SDL_Renderer *renderer, float *scaleX, float *scaleY) {
    int logical_width, logical_height;
    SDL_GetWindowSize(window, &logical_width, &logical_height);
    int physical_width, physical_height;
    if (SDL_GetRendererOutputSize(renderer, &physical_width, &physical_height)) {
        return false;
    }
    *scaleX = (float) physical_width / (float) logical_width;
    *scaleY = (float) physical_height / (float) logical_height;
    return true;
}
