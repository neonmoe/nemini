#include <SDL.h>

#include "ctx.h"
#include "net.h"
#include "error.h"

int get_refresh_rate(SDL_Window *);
bool get_scale(SDL_Window *, SDL_Renderer *, float *, float *);

int main(int argc, char *argv[]) {
    net_init();
    int result = net_request("gemini://localhost/");
    if (result != ERR_NONE) {
        SDL_Log("net_request() returned an error: %d", result);
    }
    net_free();
    return 0;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't initialize SDL: %s",
                     SDL_GetError());
        return 1;
    }

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

        float scaleX, scaleY;
        if (get_scale(window, renderer, &scaleX, &scaleY)) {
            SDL_RenderSetScale(renderer, scaleX, scaleY);
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
    int displayIndex = SDL_GetWindowDisplayIndex(window);
    if (displayIndex >= 0) {
        SDL_DisplayMode mode = {0};
        if (!SDL_GetDisplayMode(displayIndex, 0, &mode)) {
            if (mode.refresh_rate != 0) {
                return mode.refresh_rate;
            }
        }
    }
    return -1;
}

// Returns true on success, scaleX and scaleY are filled.
bool get_scale(SDL_Window *window, SDL_Renderer *renderer, float *scaleX, float *scaleY) {
    int logicalWidth, logicalHeight;
    SDL_GetWindowSize(window, &logicalWidth, &logicalHeight);
    int physicalWidth, physicalHeight;
    if (SDL_GetRendererOutputSize(renderer, &physicalWidth, &physicalHeight)) {
        return false;
    }
    *scaleX = (float) physicalWidth / (float) logicalWidth;
    *scaleY = (float) physicalHeight / (float) logicalHeight;
    return true;
}
