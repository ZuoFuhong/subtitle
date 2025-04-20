#pragma once

#include "lru_queue.h"
#include <SDL2/SDL.h>

class AudioRecorder {
public:
    AudioRecorder();
    ~AudioRecorder();

    static AudioRecorder* new_audio_recorder(LRUQueue* m_queue);

    void turn_on();

    void turn_off();

private:
    bool started{};

    LRUQueue* m_queue{};

    SDL_AudioDeviceID audio_device{};
};