#pragma once

#include <memory>
#include <string_view>
#include <map>
#include <onnxruntime_cxx_api.h>
#include "speech_recognizer.h"

class ParakeetSpeechRecognizer: public SpeechRecognizer {
public:
    explicit ParakeetSpeechRecognizer(std::string_view model_path);

    ~ParakeetSpeechRecognizer() = default;
    
    std::pair<std::string, bool> recognize_text(const float* data_chunk, unsigned int data_chunk_nlen) override;

private:
    Ort::Env env;
    Ort::SessionOptions session_options;
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeCPU);

    std::shared_ptr<Ort::Session> preprocessor;

    std::shared_ptr<Ort::Session> encoder;

    std::shared_ptr<Ort::Session> decoder_joint;

    std::map<int, std::string> vocab;

    int64_t vocab_size;

    int blank_idx;

private:
    std::vector<const char*> preprocess_input_names = {"waveforms", "waveforms_lens"};

    std::vector<const char*> preprocess_output_names = {"features", "features_lens"};

    std::vector<const char*> encoder_input_names = {"audio_signal", "length"};

    std::vector<const char*> encoder_output_names = {"outputs", "encoded_lengths"};

    std::vector<const char*> decoder_input_names = {"encoder_outputs", "targets", "target_length", "input_states_1", "input_states_2"};

    std::vector<const char*> decoder_output_names = {"outputs", "output_states_1", "output_states_2"};

private:
    std::pair<Ort::Value, Ort::Value> preprocess(const float* data_chunk, unsigned int data_chunk_nlen);

    std::pair<Ort::Value, Ort::Value> encode(Ort::Value features, Ort::Value features_lens);

    std::tuple<std::vector<float>, int, std::pair<Ort::Value, Ort::Value>> decode(std::vector<int> prev_tokens, int64_t vocab_size, int blank_idx, std::pair<Ort::Value, Ort::Value> prev_state, std::vector<float> encoder_out);
};
