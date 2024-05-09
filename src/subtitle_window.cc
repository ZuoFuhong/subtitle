#include "subtitle_window.h"
#include "utils.h"
#include <spdlog/spdlog.h>

SubtitleWindow::SubtitleWindow() = default;

SubtitleWindow::~SubtitleWindow() {
    SDL_Quit();
}

SubtitleWindow* SubtitleWindow::new_subtitle_window(LRUQueue* m_subtitle_queue) {
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_AUDIO) < 0) {
        spdlog::error("SDL could not initialize! Get_Error: {}", SDL_GetError());
        exit(-1);
    }
    auto window = new SubtitleWindow();
    window->m_subtitle_queue = m_subtitle_queue;
    return window;
}

void SubtitleWindow::run() {
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
            auto ts = utils::format_timestamp(pkt->timestamp, "%H:%M:%S");
            auto sentence = std::string(reinterpret_cast<const char*>(pkt->body), pkt->body_size);
            std::cout << "[" << ts << "] " << sentence << std::endl;
            auto sentence_zh = utils::translate_sentence(sentence);
            if (!sentence.empty()) {
                std::cout << "[" << ts << "] " << "\033[38;5;222m" << sentence_zh << "\033[0m" << std::endl;
            }
            delete pkt;
        }
        // 每秒 30 帧
        std::this_thread::sleep_for(std::chrono::microseconds(33333));
    }
}
