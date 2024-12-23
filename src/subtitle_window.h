#pragma once

#include "audio_recorder.h"
#include <string_view>

// 字幕窗口
class SubtitleWindow {
public:
    SubtitleWindow();

    ~SubtitleWindow();

    static SubtitleWindow* new_subtitle_window(LRUQueue *m_queue);

    void run();

private:
    // 字幕队列
    LRUQueue* m_subtitle_queue{};

private:
    // 双语翻译
    static std::string translate_sentence(std::string_view sentence);
};