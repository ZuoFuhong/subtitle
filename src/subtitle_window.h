#pragma once

#include "audio_recorder.h"
#include <SDL2/SDL_ttf.h>

// 字幕窗口
class SubtitleWindow {
public:
    SubtitleWindow();

    ~SubtitleWindow();

    static SubtitleWindow* new_subtitle_window(LRUQueue *m_queue);

    void run();

private:

    TTF_Font* m_font{};

    SDL_Window* sdl_window{};

    SDL_Renderer* renderer{};

    // 字幕队列
    LRUQueue* m_subtitle_queue{};
};