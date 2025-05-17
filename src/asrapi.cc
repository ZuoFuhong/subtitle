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

#include <vector>
#include <cstring>
#include <thread>
#include <onnxruntime_cxx_api.h>
#include <whisper.h>
#include "asrapi.h"
#include "utils.h"
#include "audio_activity_detector.h"
#include "../third_party/json.hpp"

struct Speech {
    int start{};
    int end{};
    std::string text;
};

static void cb_log_disable(enum ggml_log_level , const char * , void * ) { }

class ASRSession {
public:
    ASRSession() {
        vad_detector = new AudioActivityDetector("../resources/model/silero_vad.onnx");
        whisper_ctx = new_whisper_ctx("../resources/model/ggml-small.en.bin");
    }

    ~ASRSession() {
        whisper_free(whisper_ctx);
    }

    static whisper_context* new_whisper_ctx(std::string_view model_path) {
        whisper_log_set(cb_log_disable, nullptr);
        whisper_context_params cparams = whisper_context_default_params();
        cparams.use_gpu    = true;
        cparams.flash_attn = false;
        return whisper_init_from_file_with_params(model_path.data(), cparams);
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

    static bool sentence_has_finished(std::string_view text) {
        if (!text.empty() && !std::ispunct(text.back())) { // 非标点符号
            return false;
        }
        std::array<std::string_view, 4> suffixes = {"--", "--.", "...", ","}; // 半句话后缀
        return !std::any_of(suffixes.begin(), suffixes.end(), [text](std::string_view suffix) {
           return utils::ends_with(text, suffix);
        });
    }

    [[nodiscard]] bool get_active_state() {
        if (last_trigger_state && !triggered) {
            recognize_text();
            if (sentence_has_finished(current_speech.text)) { // 语义断句
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

    void recognize_text() {
        int n_sample = static_cast<int>(samples_buffer.size());
        recognize_text_with_whisper(samples_buffer, n_sample);
    }

    void recognize_text_with_whisper(std::vector<float> pcmf32, int n_samples) {
        int32_t n_threads = std::min(4, (int32_t) std::thread::hardware_concurrency());
        whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        wparams.print_progress   = false;
        wparams.print_special    = false;
        wparams.print_timestamps = false;
        wparams.print_realtime   = false;
        wparams.translate        = false;
        wparams.single_segment   = true;
        wparams.max_tokens       = 0;
        wparams.language         = "en";
        wparams.n_threads        = n_threads;
        // Run the inference
        if (whisper_full(whisper_ctx, wparams, pcmf32.data(), n_samples) != 0) {
            exit(EXIT_FAILURE);
        }
        const int n_segments = whisper_full_n_segments(whisper_ctx);
        if (n_segments > 0) {
            auto text = whisper_full_get_segment_text(whisper_ctx, 0);
            current_speech.text = utils::trim(text);
        } else {
            current_speech.text = "";
        }
    }
private:
    std::vector<float> samples_buffer;

    whisper_context* whisper_ctx;

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
