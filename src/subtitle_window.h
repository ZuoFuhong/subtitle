#pragma once

#include "audio_recorder.h"

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
};