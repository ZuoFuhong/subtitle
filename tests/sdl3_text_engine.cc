#include "SDL3/SDL_render.h"
#include "SDL3/SDL_video.h"
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

const int SCREEN_WIDTH = 800;

const int SCREEN_HEIGHT = 120;

const int PADDING = 40;

int main() {
    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO)) {
        std::cout << "SDL could not initialize! Get_Error: " << SDL_GetError() << std::endl;  
        exit(EXIT_FAILURE);
    }
    if (!TTF_Init()) {
        std::cout << "TTF could not initialize! Get_Error: " << SDL_GetError() << std::endl;  
        exit(EXIT_FAILURE);
    }
    TTF_Font* font = TTF_OpenFont("/System/Library/Fonts/Hiragino Sans GB.ttc", 38.0f);
    if (font == nullptr) {
        std::cout << "Couldn't open font: " << SDL_GetError() << std::endl;
        exit(EXIT_FAILURE);
    }

    SDL_Window *window;
    SDL_Renderer *renderer;
    if (!SDL_CreateWindowAndRenderer("Simple SDL3 window", SCREEN_WIDTH, SCREEN_HEIGHT,  SDL_WINDOW_METAL | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_BORDERLESS, &window, &renderer)) {
        std::cout << "SDL_CreateWindowAndRenderer failed: " << SDL_GetError() << std::endl;
        exit(EXIT_FAILURE);
    }

    TTF_TextEngine *text_engine = TTF_CreateRendererTextEngine(renderer);
    if (text_engine == nullptr) {
        std::cout << "TTF_CreateRendererTextEngine failed: " << SDL_GetError() << std::endl;
        exit(EXIT_FAILURE);
    }
    TTF_Text *text = TTF_CreateText(text_engine, font, "在自然语言处理领域，长文本摘要模型是一种关键的技术，它旨在从冗长的文章或文档中提取出关键信息，形成简洁而准确的概述。", 0);
    if (text == nullptr) {
        std::cout << "TTF_CreateText failed: " << SDL_GetError() << std::endl;
        exit(EXIT_FAILURE);
    }
    int w, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    TTF_SetTextWrapWidth(text, w - PADDING * 2);

    bool quit = false;
    SDL_Event event;
    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                quit = true;
            }
            static bool dragging = false;
            static float drag_offset_x = 0, drag_offset_y = 0;
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                dragging = true;
                drag_offset_x = event.button.x;
                drag_offset_y = event.button.y;
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
                dragging = false;
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION && dragging) {
                int win_x, win_y;
                SDL_GetWindowPosition(window, &win_x, &win_y);
                SDL_SetWindowPosition(window,
                    win_x + static_cast<int>(event.motion.x - drag_offset_x),
                    win_y + static_cast<int>(event.motion.y - drag_offset_y)
                );
            }
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        TTF_DrawRendererText(text, PADDING, PADDING);
        SDL_RenderPresent(renderer);

        // 控制 60 FPS
        SDL_Delay(1000 / 60);
    }
    TTF_DestroyText(text);
    TTF_DestroyRendererTextEngine(text_engine);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    SDL_Quit();
    return 0;
}