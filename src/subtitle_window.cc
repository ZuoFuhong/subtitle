#include "subtitle_window.h"
#include <spdlog/spdlog.h>
#include <SDL2/SDL_image.h>


const int WINDOW_WIDTH = 1280;

const int WINDOW_HEIGHT = 720;

SubtitleWindow::SubtitleWindow() = default;

SubtitleWindow::~SubtitleWindow() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(sdl_window);
    IMG_Quit();
    SDL_Quit();
    TTF_CloseFont(m_font);
    TTF_Quit();
}

SubtitleWindow* SubtitleWindow::new_subtitle_window(LRUQueue* m_subtitle_queue) {
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_AUDIO) < 0) {
        spdlog::error("SDL could not initialize! Get_Error: {}", SDL_GetError());
        exit(-1);
    }
    if (IMG_Init(IMG_INIT_PNG) == 0) {
        spdlog::error("SDL2_image could not initialize! SDL_Error: {}", IMG_GetError());
        exit(-1);
    }
    if (TTF_Init() < 0) {
        spdlog::error("TTF could not initialize! TTF_Error: {}", TTF_GetError());
        exit(-1);
    }
    TTF_Font* font = TTF_OpenFont("../resources/fonts/Alibaba-PuHuiTi-Regular.ttf", 18);
    if (font == nullptr) {
        spdlog::error("Failed to load font! TTF_Error: {}", TTF_GetError());
        exit(-1);
    }
    SDL_Window *sdl_window = SDL_CreateWindow("Subtitle", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (sdl_window == nullptr) {
        spdlog::error("Window could not be created! Get_Error: {}", SDL_GetError());
        exit(-1);
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        spdlog::error("Renderer could not be created! Get_Error: {}", SDL_GetError());
        exit(-1);
    }
    auto window = new SubtitleWindow();
    window->m_font = font;
    window->sdl_window = sdl_window;
    window->renderer = renderer;
    window->m_subtitle_queue = m_subtitle_queue;
    return window;
}

void SubtitleWindow::run() {
    SDL_Surface* text_surface = TTF_RenderUTF8_Blended(m_font, "测试字幕 12345 Are you OK!", SDL_Color{ 0, 0, 0, 255 });
    SDL_Texture* text_tex = SDL_CreateTextureFromSurface(renderer, text_surface);
    if (text_tex == nullptr) {
        std::cout << "Error creating texture: " << SDL_GetError() << std::endl;
        exit(-1);
    }
    SDL_Rect text_rect{ 100, 100, text_surface->w, text_surface->h };

    bool quit = false;
    SDL_Event event;
    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) {
                spdlog::info("space keydown event");
            }
        }
        if (m_subtitle_queue->size() > 0) {
            auto pkt = m_subtitle_queue->pop();
            auto text = std::string(reinterpret_cast<const char*>(pkt->body), pkt->body_size);
            spdlog::info("window: {}", text);
            delete pkt;
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, text_tex, nullptr, &text_rect);
        SDL_RenderPresent(renderer);
        // 每秒 30 帧
        std::this_thread::sleep_for(std::chrono::microseconds(33333));
    }
}
