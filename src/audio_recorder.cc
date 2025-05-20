// Copyright (c) 2025 Mars Zuo
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "audio_recorder.h"
#include <spdlog/spdlog.h>
#include <SDL3/SDL_audio.h>
#include "utils.h"

// Number of samples for 20ms audio. 
const int FRAME_SIZE = 320 * 2; // For s16 format, each sample is 2 bytes (16 bits).

AudioRecorder::AudioRecorder(LRUQueue* audio_queue, SDL_AudioDeviceID devid) {
    SDL_AudioSpec desired_spec;
    SDL_zero(desired_spec);
    desired_spec.freq = 16000;
    desired_spec.format = SDL_AUDIO_S16LE;
    desired_spec.channels = 1;
    SDL_AudioStream* audio_stream = SDL_OpenAudioDeviceStream(devid, &desired_spec, nullptr, audio_queue);
    if (audio_stream == nullptr) {
        spdlog::error("Failed to open audio stream, error: {}", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    m_audio_stream = audio_stream;
    m_audio_queue = audio_queue;

    std::chrono::milliseconds ms = std::chrono::milliseconds(20);
    m_timer.start(ms, &AudioRecorder::on_timer_pull_audio, this);
}

void AudioRecorder::turn_on() {
    if (started) {
        return;
    }
    started = true;
    SDL_ClearAudioStream(m_audio_stream);
    SDL_ResumeAudioStreamDevice(m_audio_stream);
}

void AudioRecorder::turn_off() {
    started = false;
    SDL_PauseAudioStreamDevice(m_audio_stream);
}

void AudioRecorder::on_timer_pull_audio() {
    while(true) {
        if (!started) {
            break;
        }
        int available_bytes = SDL_GetAudioStreamAvailable(m_audio_stream);
        if (available_bytes < 0) {
            spdlog::error("Failed to get audio stream available size, error: {}", SDL_GetError());
            exit(EXIT_FAILURE);
        }
        if (available_bytes < FRAME_SIZE) {
            break;
        }
        auto pcm_data = new uint8_t[FRAME_SIZE];
        auto pkt = new Packet();
        pkt->type = AUDIO;
        pkt->timestamp = utils::current_timestamp();
        pkt->body = pcm_data;
        pkt->body_size = FRAME_SIZE;
        if (SDL_GetAudioStreamData(m_audio_stream, pcm_data, FRAME_SIZE) < 0) {
            spdlog::error("Failed to get audio stream data, error: {}", SDL_GetError());
            exit(EXIT_FAILURE);
        }
        m_audio_queue->push(pkt);
    }
}
