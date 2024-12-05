#include "audio_recorder.h"
#include "utils.h"
#include <spdlog/spdlog.h>

// 采样数 20ms 音频
const int FRAME_SIZE = 320;

// 本地音频驱动
const char* AUDIO_DEVICE_NAME = "Loopback";

AudioRecorder::AudioRecorder() = default;

AudioRecorder* AudioRecorder::new_audio_recorder(LRUQueue* m_queue) {
    SDL_AudioSpec desired_spec;
    SDL_AudioSpec obtained_spec;
    SDL_zero(desired_spec);
    SDL_zero(obtained_spec);
    // 音频采样参数
    desired_spec.freq = 16000;
    desired_spec.format = AUDIO_S16;
    desired_spec.channels = 1;
    desired_spec.samples = FRAME_SIZE; // 回调函数每次处理 20ms 的音频
    desired_spec.userdata = m_queue;
    desired_spec.callback =  [](void * userdata, uint8_t* stream, int nlen) {
        auto audio_queue = (LRUQueue *)userdata;

        auto pcm_data = new uint8_t[nlen];
        memcpy(pcm_data, stream, nlen);

        auto pkt = new Packet();
        pkt->type = AUDIO;
        pkt->timestamp = utils::current_timestamp();
        pkt->body = pcm_data;
        pkt->body_size = nlen;
        audio_queue->push(pkt);
    };
    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(
            AUDIO_DEVICE_NAME, SDL_TRUE, &desired_spec, &obtained_spec, 0);
    if (audio_device == 0) {
        spdlog::error("Failed to open audio device, error: {}", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    auto audio_recorder = new AudioRecorder();
    audio_recorder->m_queue = m_queue;
    audio_recorder->audio_device = audio_device;
    return audio_recorder;
}

void AudioRecorder::turn_on() {
    if (started) {
        return;
    }
    started = true;
    SDL_PauseAudioDevice(audio_device, 0);
}

void AudioRecorder::turn_off() {
    started = false;
    SDL_PauseAudioDevice(audio_device, 1);
}

AudioRecorder::~AudioRecorder() {
    SDL_CloseAudioDevice(audio_device);
}
