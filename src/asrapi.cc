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

#include <string>
#include <vector>
#include <cstring>
#include <onnxruntime_cxx_api.h>
#include "asrapi.h"
#include "audio_activity_detector.h"
#include "whisper_speech_recognizer.h"
#include "../third_party/json.hpp"

struct Speech {
    int start{};
    int end{};
    std::string text;
};

class ASRSession {
public:
    ASRSession() {
        vad_detector = new AudioActivityDetector("../resources/model/silero_vad.onnx");
        speech_recognizer = new WhisperSpeechRecognizer("../resources/model/ggml-small.en.bin");
    }

    void detect_vad(const float* data, unsigned int nlen) {
        float speech_prob = vad_detector->predict(data, nlen);
        current_sample += window_size_samples;
        // 语音活动
        if (speech_prob >= threshold) {
            if (temp_end != 0) { // 容忍一些静默片段
                temp_end = 0;
            }
            if (!triggered) {
                triggered = true;
                if (!has_not_finished) {
                    current_speech = Speech{};
                    current_speech.start = current_sample - window_size_samples;
                }
            }
            return;
        }
        // 语音静默
        if (speech_prob < std::max(threshold - 0.15, 0.1) && triggered) {
            if (current_sample - current_speech.start < min_speech_samples) {
                return; // 发言片段太短
            }
            if (temp_end == 0) {
                temp_end = current_sample; // 记录静默的位置
            }
            if (current_sample - temp_end < min_silence_samples) {
                // 在每个语音块结束时, 等待 min_silence_samples 再将其分离
            } else {
                current_speech.end = temp_end; // 丢弃末尾片段
                temp_end = 0;
                triggered = false;
            }
        }
    }

    std::pair<std::string, bool> recognize_text() {
        return speech_recognizer->recognize_text(samples_buffer.data(), samples_buffer.size());
    }

    [[nodiscard]] bool get_active_state() {
        if (last_trigger_state && !triggered) {
            auto result = recognize_text();
            if (result.second) {
                current_speech.text = result.first;
                has_not_finished = false;
            } else {
                last_trigger_state = triggered;
                has_not_finished = true;
                return has_not_finished;
            }
        }
        last_trigger_state = triggered;
        if (has_not_finished) {
            return true;
        }
        return triggered;
    }

    void push_buffer(const float* data, unsigned int nlen) {
        if (triggered || has_not_finished) {
            samples_buffer.insert(samples_buffer.end(), data, data + nlen);
        } else {
            // 保留一个窗口
            samples_buffer.assign(data, data + nlen);
        }
    }

    void reset_states() {
        samples_buffer.clear();
    }

    std::string get_speech() {
        nlohmann::json speech;
        speech["se_id"] = ++sequence;
        speech["start"] = current_speech.start;
        speech["end"] = current_speech.end;
        speech["text"] = current_speech.text;
        return speech.dump();
    }

private:
    SpeechRecognizer* speech_recognizer;

    std::vector<float> samples_buffer;

    Speech current_speech;

    int sequence = 0;

private:
    AudioActivityDetector* vad_detector;

    int window_size_samples = 512;  // Assign when init, support 256 512 768 for 8k; 512 1024 1536 for 16k.

    float threshold = 0.5;

    bool last_trigger_state = false;

    bool has_not_finished = false;

    bool triggered = false;

    int min_silence_samples = 16 * 100; // Minimum silence duration: 100 ms

    int min_speech_samples = 16 * 2000; // Maximum speech segment duration is 2 seconds

    int current_sample = 0;

    int temp_end = 0;
};

ASRCode ASR_create_session(HANDLE& session) {
    auto m_session = new ASRSession();
    session = static_cast<void*>(m_session);
    return ERROR_OK;
}

ASRCode ASR_begin_session(HANDLE session) {
    if (session == nullptr) {
        return ERROR_PARA;
    }
    return ERROR_OK;
}

ASRCode ASR_end_session(HANDLE session) {
    if (session == nullptr) {
        return ERROR_PARA;
    }
    auto m_session = static_cast<ASRSession*>(session);
    m_session->reset_states();
    return ERROR_OK;
}

ASRCode ASR_push_buffer(HANDLE session, const float* pdata, unsigned int nlen) {
    if (session == nullptr) {
        return ERROR_PARA;
    }
    auto m_session = static_cast<ASRSession*>(session);
    m_session->push_buffer(pdata, nlen);
    m_session->detect_vad(pdata, nlen);
    return ERROR_OK;
}

ASRCode ASR_get_vad_state(HANDLE session, int* state) {
    if (session == nullptr || state == nullptr) {
        return ERROR_PARA;
    }
    auto m_session = static_cast<ASRSession*>(session);
    *state = m_session->get_active_state() ? 1 : 0;
    return ERROR_OK;
}

ASRCode ASR_get_result(HANDLE session, std::string& res) {
    if (session == nullptr) {
        return ERROR_PARA;
    }
    auto m_session = static_cast<ASRSession*>(session);
    res = m_session->get_speech();
    return ERROR_OK;
}
