#pragma once

#include "lru_queue.h"
#include <SDL2/SDL.h>

// 录制音频
class AudioRecorder {
public:
    AudioRecorder();
    ~AudioRecorder();

    static AudioRecorder* new_audio_recorder(LRUQueue* m_queue);

    // 开启音频
    void turn_on();

    // 关闭音频
    void turn_off();

private:
    // 开启标记
    bool started{};

    // 音频队列
    LRUQueue* m_queue{};

    SDL_AudioDeviceID audio_device{};
};