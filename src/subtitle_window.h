#pragma once

#include "lru_queue.h"

class SubtitleWindow {
public:
    SubtitleWindow();

    ~SubtitleWindow();

    static SubtitleWindow* new_subtitle_window(LRUQueue *m_queue);

    void run();

private:
    LRUQueue* m_subtitle_queue{};
};