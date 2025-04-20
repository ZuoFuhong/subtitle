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
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <iostream>
#include <thread>
#include "offline_convert_timer.h"
#include "asrapi.h"
#include "utils.h"
#include "../third_party/json.hpp"

// 音频包 20ms
const int FRAME_DURATION = 32;

// 语音活动窗口
const int WINDOW_SIZE_SAMPLES = 512;

OfflineConvertTimer* OfflineConvertTimer::new_convert_timer(LRUQueue* audio_queue, LRUQueue* subtitle_queue) {
    auto timer = new OfflineConvertTimer();
    timer->m_audio_queue = audio_queue;
    timer->m_subtitle_queue = subtitle_queue;
    return timer;
}

void OfflineConvertTimer::start() {
    HANDLE session;
    auto code = ASR_create_session(session);
    if (code != ASRCode::ERROR_OK) {
        std::cerr << "ASR_create_session err code=" << code << std::endl;
        exit(EXIT_FAILURE);
    }
    int nstate = 0; // 0-没说话 1-在说话
    int nstatelast = 0;
    std::vector<float> samples_buffer;
    while(true) {
        if (m_audio_queue->empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DURATION));
            continue;
        }
        // 归一化处理
        Packet* audio_pkt = m_audio_queue->pop();
        for (int i = 0; i < audio_pkt->body_size; i+=sizeof(int16_t)) {
            auto sample = static_cast<int16_t>((audio_pkt->body[i + 1] << 8) | audio_pkt->body[i]);
            samples_buffer.push_back(static_cast<float>(sample) / 32768);
        }
        delete audio_pkt;
        if (samples_buffer.size() < WINDOW_SIZE_SAMPLES) {
            continue;
        }
        // 语音识别
        code = ASR_push_buffer(session, samples_buffer.data(),  WINDOW_SIZE_SAMPLES);
        if (code != ASRCode::ERROR_OK) {
            std::cerr << "ASR_push_buffer err code=" << code << std::endl;
            exit(EXIT_FAILURE);
        }
        code = ASR_get_vad_state(session, &nstate);
        if (code != ASRCode::ERROR_OK) {
            std::cerr << "ASR_get_vad_state err code=" << code << std::endl;
            exit(EXIT_FAILURE);
        }
        std::string result;
        if (!nstatelast && nstate) {
            // 之前没说话, 现在说话
            // 新起一句
            code = ASR_begin_session(session);
            if (code != ASRCode::ERROR_OK) {
                std::cerr << "ASR_begin_session err code=" << code << std::endl;
                exit(EXIT_FAILURE);
            }
        } else if (nstatelast && !nstate) {
            // 之前在说话, 现在没说话
            code = ASR_end_session(session);
            if (code != ERROR_OK) {
                std::cerr << "ASR_end_session err code=" << code << std::endl;
                exit(EXIT_FAILURE);
            }
            // 获取稳态句子
            code = ASR_get_result(session, result);
            if (code != ERROR_OK) {
                std::cout << "ASR_get_result err code=" << code << std::endl;
                exit(EXIT_FAILURE);
            }
            if (!result.empty()) {
                auto object = nlohmann::json::parse(result);
                if (object.contains("text")) {
                    std::string text = object["text"];
                    size_t str_size = text.size();
                    auto body = new uint8_t[str_size];
                    std::memcpy(body, text.data(), str_size);
                    auto pkt = new Packet();
                    pkt->type = SUBTITLE;
                    pkt->timestamp = utils::current_timestamp();
                    pkt->body = body;
                    pkt->body_size = str_size;
                    m_subtitle_queue->push(pkt);
                }
            }
        }
        nstatelast = nstate;
        samples_buffer.erase(samples_buffer.begin(), samples_buffer.begin() + WINDOW_SIZE_SAMPLES);
    }
}