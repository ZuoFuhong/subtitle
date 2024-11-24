#pragma once

#include "lru_queue.h"

class ConvertTimer {
public:
    ConvertTimer();

    ~ConvertTimer();

    static ConvertTimer* new_convert_timer(LRUQueue* m_audio_queue, LRUQueue* m_subtitle_queue);

    [[noreturn]] void start();

    void set_target(std::string ip, unsigned short port);

private:

    LRUQueue* m_audio_queue{};

    LRUQueue* m_subtitle_queue{};

    std::string m_ip;

    unsigned short m_port = 0;
};