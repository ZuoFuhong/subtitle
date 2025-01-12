#include <vector>
#include <cstring>
#include <memory>
#include <thread>
#include <onnxruntime_cxx_api.h>
#include <whisper.h>
#include "asrapi.h"
#include "utils.h"

struct Speech {
    int start;
    int end;
    std::string text;
};

static void cb_log_disable(enum ggml_log_level , const char * , void * ) { }

class ASRSession {
public:
    explicit ASRSession(int sample_rate = 16000, float threshold = 0.4,
                        int window_size_samples = 512, int min_silence_duration_ms = 96, int min_speech_duration_ms = 1000) {
        init_whisper_model("../resources/model/ggml-small.en.bin");
        init_onnx_model("../resources/model/silero_vad.onnx");
        m_window_size_samples = window_size_samples;
        m_threshold = threshold;
        sr_per_ms = sample_rate / 1000;
        min_silence_samples = sr_per_ms * min_silence_duration_ms;
        min_speech_samples = sr_per_ms * min_speech_duration_ms;

        input.resize(m_window_size_samples);
        input_node_dims[0] = 1;
        input_node_dims[1] = m_window_size_samples;

        _state.resize(size_state);
        sr.resize(1);
        sr[0] = sample_rate;
    }

    ~ASRSession() {
        samples_buffer.clear();
        whisper_free(whisper_ctx);
    }

    void predict(const float* data, unsigned int nlen) {
        // Infer
        // Create ort tensors
        std::copy(data, data + nlen, input.begin());
        Ort::Value input_ort = Ort::Value::CreateTensor<float>(
                memory_info, input.data(), input.size(), input_node_dims, 2);
        Ort::Value state_ort = Ort::Value::CreateTensor<float>(
                memory_info, _state.data(), _state.size(), state_node_dims, 3);
        Ort::Value sr_ort = Ort::Value::CreateTensor<int64_t>(
                memory_info, sr.data(), sr.size(), sr_node_dims, 1);

        // Clear and add inputs
        ort_inputs.clear();
        ort_inputs.emplace_back(std::move(input_ort));
        ort_inputs.emplace_back(std::move(state_ort));
        ort_inputs.emplace_back(std::move(sr_ort));

        // Infer
        ort_outputs = session->Run(Ort::RunOptions{nullptr}, input_node_names.data(),
                                   ort_inputs.data(), ort_inputs.size(),
                                   output_node_names.data(), output_node_names.size());

        // Output probability & update h,c recursively
        float speech_prob = ort_outputs[0].GetTensorMutableData<float>()[0];
        auto stateN = ort_outputs[1].GetTensorMutableData<float>();
        std::memcpy(_state.data(), stateN, size_state * sizeof(float));

        // Push forward sample index
        current_sample += m_window_size_samples;

        // 语音活动
        if (speech_prob >= m_threshold) {
            if (temp_end != 0) { // 容忍一些静默片段
                temp_end = 0;
            }
            if (!triggered) {
                triggered = true;
                current_speech = Speech{};
                current_speech.start = current_sample - m_window_size_samples;
            }
            return;
        }
        // 语音静默
        if (speech_prob < std::max(m_threshold - 0.15, 0.1) && triggered) {
            if (temp_end == 0) {
                temp_end = current_sample; // 记录静默的位置
            }
            if (current_sample - temp_end < min_silence_samples) {
                // 在每个语音块结束时, 等待 min_silence_samples 再将其分离
            } else if (current_sample - current_speech.start < min_speech_samples) {
                // 发言片段太短
            } else {
                current_speech.end = temp_end; // 丢弃末尾片段
                temp_end = 0;
                triggered = false;
            }
        }
    }

    [[nodiscard]] bool get_active_state() {
        if (last_trigger_state && !triggered) {
            recognize_text();
            std::string_view text = current_speech.text;
            if (!text.empty() && std::ispunct(text.back()) && text.back() != '-') { // 语义断句
                // nothing to do
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

    Speech get_speech() {
        return current_speech;
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
    Ort::Env env;
    Ort::SessionOptions session_options;
    std::shared_ptr <Ort::Session> session = nullptr;
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeCPU);

    // Onnx model
    // Inputs
    std::vector<Ort::Value> ort_inputs;

    std::vector<const char *> input_node_names = {"input", "state", "sr"};
    std::vector<float> input;
    unsigned int size_state = 2 * 1 * 128;
    std::vector<float> _state;
    std::vector<int64_t> sr;

    int64_t input_node_dims[2] = {};
    const int64_t state_node_dims[3] = {2, 1, 128};
    const int64_t sr_node_dims[1] = {1};

    // Outputs
    std::vector<Ort::Value> ort_outputs;
    std::vector<const char *> output_node_names = {"output", "stateN"};

private:
    int m_window_size_samples = 0;  // Assign when init, support 256 512 768 for 8k; 512 1024 1536 for 16k.
    int sr_per_ms;   // Assign when init, support 8 or 16
    float m_threshold;
    int min_silence_samples;
    int min_speech_samples;

    bool triggered = false;
    bool last_trigger_state = false;
    bool has_not_finished = false;

    int temp_end = 0;
    int current_sample = 0;
    std::vector<float> samples_buffer;

    Speech current_speech{};

private:
    whisper_context* whisper_ctx = nullptr;

private:
    void init_onnx_model(const std::string& model_path) {
        init_engine_threads();
        session = std::make_shared<Ort::Session>(env, model_path.c_str(), session_options);
    }

    void init_engine_threads() {
        session_options.SetIntraOpNumThreads(1);
        session_options.SetInterOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    }

    void init_whisper_model(const std::string& model_path) {
        whisper_log_set(cb_log_disable, nullptr);
        whisper_context_params cparams = whisper_context_default_params();
        cparams.use_gpu    = true;
        cparams.flash_attn = false;
        whisper_ctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    }
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
    m_session->predict(pdata, nlen); // Vad 推理
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
    auto speech = m_session->get_speech();
    res = speech.text;
    return ERROR_OK;
}
