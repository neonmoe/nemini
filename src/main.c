#include <SDL.h>
#include "ctx.h"

int get_refresh_rate(SDL_Window*);

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window;
    SDL_Renderer *renderer;
    if (SDL_CreateWindowAndRenderer(320, 240, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window and renderer: %s", SDL_GetError());
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
