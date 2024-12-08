#pragma once

#include <onnxruntime/onnxruntime_cxx_api.h>

struct Speech {
    int start;
    int end;
};

class VadSession {
public:
    explicit VadSession(const std::string& model_path,
                int freq = 16000, int windows_frame_size = 32,
                float Threshold = 0.5, int min_silence_duration_ms = 100,
                int speech_pad_ms = 30, int min_speech_duration_ms = 250,
                int max_speech_duration_s = 5);

    void process(const std::vector<float>& input_wav);

    [[nodiscard]] std::vector<Speech> get_speeches() const;

private:
    Ort::Env env;
    Ort::SessionOptions session_options;
    std::shared_ptr <Ort::Session> session = nullptr;
    Ort::AllocatorWithDefaultOptions allocator;
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeCPU);

private:
    void init_engine_threads(int inter_threads, int intra_threads);
    void init_onnx_model(const std::string& model_path);
    void reset_states();
    void predict(const std::vector<float> &data);

private:
    int window_size_samples = 0;  // Assign when init, support 256 512 768 for 8k; 512 1024 1536 for 16k.
    int sample_rate;  //Assign when init support 16000 or 8000
    int sr_per_ms;   // Assign when init, support 8 or 16
    float threshold;
    int min_silence_samples;
    int min_silence_samples_at_max_speech;
    int min_speech_samples;
    int max_speech_samples;
    int speech_pad_samples;
    int audio_length_samples = 0;

    // model states
    bool triggered = false;
    int temp_end = 0;
    int current_sample = 0;
    // MAX 4294967295 samples / 8sample per ms / 1000 / 60 = 8947 minutes
    int prev_end = 0;
    int next_start = 0;

    // Output
    std::vector<Speech> speeches;
    Speech current_speech{};

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
};