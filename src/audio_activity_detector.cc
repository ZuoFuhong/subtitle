#include "audio_activity_detector.h"
#include <array>

AudioActivityDetector::AudioActivityDetector(std::string_view model_path) {
    _state.resize(state_size);
    sr.resize(1);
    sr[0] = 16000; // Must match input audio sample rate

    init_onnx_model(model_path);
}

void AudioActivityDetector::init_onnx_model(std::string_view model_path) {
    Ort::Env env;
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);
    session_options.SetInterOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    session = new Ort::Session(env, model_path.data(), session_options);
}

float AudioActivityDetector::predict(const float* data_chunk, unsigned int data_chunk_nlen) {
    std::vector<float> new_data(effective_window_size, 0.0f);
    std::copy(_context.begin(), _context.end(), new_data.begin());
    std::copy(data_chunk, data_chunk + data_chunk_nlen, new_data.begin() + context_samples);
    input = new_data;

    Ort::Value input_ort = Ort::Value::CreateTensor<float>(
            memory_info, input.data(), input.size(), input_node_dims.data(), input_node_dims.size());
    Ort::Value state_ort = Ort::Value::CreateTensor<float>(
            memory_info, _state.data(), _state.size(), state_node_dims.data(), state_node_dims.size());
    Ort::Value sr_ort = Ort::Value::CreateTensor<int64_t>(
            memory_info, sr.data(), sr.size(), sr_node_dims, 1);

    std::array<Ort::Value, 3> inputs_tensors = {std::move(input_ort), std::move(state_ort), std::move(sr_ort)};
    std::vector<Ort::Value> ort_outputs = session->Run(Ort::RunOptions{nullptr}, input_names.data(),
                                inputs_tensors.data(), input_names.size(),
                                output_names.data(), output_names.size());

    float speech_prob = ort_outputs[0].GetTensorMutableData<float>()[0];
    auto stateN = ort_outputs[1].GetTensorMutableData<float>();
    std::memcpy(_state.data(), stateN, state_size * sizeof(float));
    return speech_prob;
}
