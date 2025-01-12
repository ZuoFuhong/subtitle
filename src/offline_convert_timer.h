#pragma once
#include "lru_queue.h"

class OfflineConvertTimer {
public:
    OfflineConvertTimer() = default;
    ~OfflineConvertTimer() = default;

    static OfflineConvertTimer* new_convert_timer(LRUQueue* audio_queue, LRUQueue* subtitle_queue);

    [[noreturn]] void start();

private:
    LRUQueue* m_audio_queue{};

    LRUQueue* m_subtitle_queue{};
};

