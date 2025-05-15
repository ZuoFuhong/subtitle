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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <regex>
#include <string>
#include <utility>
#include <map>
#include <tuple>
#include <sstream>
#include <fstream>
#include <format>
#include <onnxruntime_cxx_api.h>
#include "../third_party/wav.h"
#include "../third_party/clipp.h"

std::map<int, std::string> load_vocab(std::string_view vocab_path) {
    std::map<int, std::string> vocab;
    std::ifstream infile(vocab_path.data());
    std::string line;
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::string token;
        int id;
        if (iss >> token >> id) {
            // Replace Unicode character \u2581 with space
            size_t pos;
            while ((pos = token.find("\u2581")) != std::string::npos) {
                token.replace(pos, 3, " ");
            }
            vocab[id] = token;
        }
    }
    return vocab;
}

int find_blank_idx(std::string_view token, const std::map<int, std::string>& vocab) {
    auto it = std::find_if(vocab.begin(), vocab.end(), [&token](const auto& pair) {
        return pair.second == token;
    });
    if (it != vocab.end()) {
        return it->first;
    }
    std::cerr << "Token not found in vocabulary: " << token << std::endl;
    return -1;
}

// Preprocessing: Convert audio to features
std::pair<Ort::Value, Ort::Value> preprocess(std::string_view model_path, std::vector<float> audio_wav) {
    auto waveforms_size = static_cast<int64_t>(audio_wav.size());
    std::vector<float>   waveforms = std::move(audio_wav);
    std::vector<int64_t> waveforms_shape = {1, waveforms_size};
    std::vector<int64_t> waveforms_lens_shape = {1};
    std::vector<int64_t> waveforms_lens = {waveforms_size};

    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeCPU);
    Ort::Value input_waveforms = Ort::Value::CreateTensor<float>(memory_info, 
        waveforms.data(), waveforms.size(), waveforms_shape.data(), waveforms_shape.size());
    Ort::Value input_waveforms_lens = Ort::Value::CreateTensor<int64_t>(memory_info, 
        waveforms_lens.data(), waveforms_lens.size(), waveforms_lens_shape.data(), waveforms_lens_shape.size());

    std::vector<const char*> input_names = {"waveforms", "waveforms_lens"};
    std::vector<const char*> output_names = {"features", "features_lens"};
    std::array<Ort::Value, 2> input_tensors = {std::move(input_waveforms), std::move(input_waveforms_lens)};

    Ort::Env env;
    Ort::SessionOptions session_options;
    Ort::Session preprocessor(env, model_path.data(), session_options);
    auto output_tensors = preprocessor.Run(Ort::RunOptions{nullptr}, 
        input_names.data(), input_tensors.data(), input_names.size(), 
        output_names.data(), output_names.size());
    return {std::move(output_tensors[0]), std::move(output_tensors[1])};
}

// Encoder inference: convert features to high-dimensional representations
std::pair<Ort::Value, Ort::Value> encode(std::string_view model_path, Ort::Value features, Ort::Value features_lens) {
    std::vector<const char*> input_names = {"audio_signal", "length"};
    std::vector<const char*> output_names = {"outputs", "encoded_lengths"};
    std::array<Ort::Value, 2> input_tensors = {std::move(features), std::move(features_lens)};

    Ort::Env env;
    Ort::SessionOptions session_options;
    Ort::Session encoder(env, model_path.data(), session_options);
    auto output_tensors = encoder.Run(
        Ort::RunOptions{nullptr},
        input_names.data(), input_tensors.data(), input_names.size(),
        output_names.data(), output_names.size());
    return {std::move(output_tensors[0]), std::move(output_tensors[1])};
}

std::pair<Ort::Value, Ort::Value> create_state() {
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeCPU);
    std::vector<int64_t> state_shape = {2, 1, 640};
    Ort::Value state_1 = Ort::Value::CreateTensor<float>(
        memory_info, new float[1280]{0}, 1280, state_shape.data(), state_shape.size());
    Ort::Value state_2 = Ort::Value::CreateTensor<float>(
        memory_info, new float[1280]{0}, 1280, state_shape.data(), state_shape.size());
    return {std::move(state_1), std::move(state_2)};
}

std::pair<Ort::Value, Ort::Value> clone_state(std::pair<Ort::Value, Ort::Value>& state) {
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeCPU);
    std::vector<int64_t> state_shape = {2, 1, 640};
    auto state_1_copy = new float[1280];
    auto state_2_copy = new float[1280];
    std::memcpy(state_1_copy, state.first.GetTensorMutableData<float>(), 5120);
    std::memcpy(state_2_copy, state.second.GetTensorMutableData<float>(), 5120);
    Ort::Value state_1 = Ort::Value::CreateTensor<float>(
        memory_info, state_1_copy, 1280, state_shape.data(), state_shape.size()); 
    Ort::Value state_2 = Ort::Value::CreateTensor<float>(
        memory_info, state_2_copy, 1280, state_shape.data(), state_shape.size()); 
    return {std::move(state_1), std::move(state_2)};
}

int argmax(const std::vector<float>& v) {
    return static_cast<int>(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
}

std::tuple<std::vector<float>, int, std::pair<Ort::Value, Ort::Value>> decode(Ort::Session* decoder_joint, 
    std::vector<int> prev_tokens, int64_t vocab_size, int blank_idx, 
    std::pair<Ort::Value, Ort::Value> prev_state, std::vector<float> encoder_out) {
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeCPU);
    std::vector<int64_t> encoder_out_shape = {1, static_cast<int64_t>(encoder_out.size()), 1};
    Ort::Value input_encoder_outputs = Ort::Value::CreateTensor<float>(
        memory_info, encoder_out.data(), encoder_out.size(), encoder_out_shape.data(), encoder_out_shape.size());
    int target = static_cast<int>(blank_idx);
    if (!prev_tokens.empty()) {
        target = prev_tokens.back();
    }
    std::vector<int> targets = { target };
    std::vector<int64_t> target_shape = {1, 1};
    Ort::Value input_targets = Ort::Value::CreateTensor<int>(
        memory_info, targets.data(), targets.size(), target_shape.data(), target_shape.size());
    std::vector<int32_t> target_length = {1};
    std::vector<int64_t> target_length_shape = {1};
    Ort::Value input_target_length = Ort::Value::CreateTensor<int32_t>(
        memory_info, target_length.data(), target_length.size(), target_length_shape.data(), target_length_shape.size());
    std::vector<const char*> input_names = {"encoder_outputs", "targets", "target_length", "input_states_1", "input_states_2"};
    std::vector<const char*> output_names = {"outputs", "output_states_1", "output_states_2"};
    std::array<Ort::Value, 5> input_tensors = {std::move(input_encoder_outputs), std::move(input_targets), std::move(input_target_length), std::move(prev_state.first), std::move(prev_state.second)};

    auto output_tensors = decoder_joint->Run(Ort::RunOptions{nullptr},
    input_names.data(), input_tensors.data(), input_names.size(),
    output_names.data(), output_names.size());

    auto decoder_out_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
    auto decoder_out_ptr = output_tensors[0].GetTensorMutableData<float>();
    int64_t decoder_num = 1;
    for (auto dim : decoder_out_shape) {
        decoder_num *= dim;
    }
    std::vector<float> decoder_out(decoder_out_ptr, decoder_out_ptr + decoder_num);
    std::vector<float> output1(decoder_out.begin(), decoder_out.begin() + vocab_size);
    std::vector<float> output2(decoder_out.begin() + vocab_size, decoder_out.end());
    std::pair<Ort::Value, Ort::Value> state = {std::move(output_tensors[1]), std::move(output_tensors[2])};
    return {output1, argmax(output2), std::move(state)};
}

std::string decode_space_pattern(std::string tokens_str) {
    static const std::regex re1(R"(^\s)");
    static const std::regex re2(R"(\s(?!\b))");
    static const std::regex re3(R"((\s)\b)");
    std::string result = std::move(tokens_str);
    result = std::regex_replace(result, re1, "");
    result = std::regex_replace(result, re2, "");
    result = std::regex_replace(result, re3, " ");
    return result;
}

std::string tokens_to_text(const std::vector<int>& tokens, const std::map<int, std::string>& vocab) {
    std::string joined;
    for (int id : tokens) {
        auto it = vocab.find(id);
        if (it != vocab.end()) {
            joined += it->second;
        }
    }
    return decode_space_pattern(joined);
}

/**
 * Main entry point for the Parakeet TDT ASR test application.
 *
 * This program performs automatic speech recognition (ASR) on a given audio file using ONNX models.
 * It supports command-line options for specifying the model directory and audio file.
 *
 * Workflow:
 * 1. Parses command-line arguments for model path and audio file.
 * 2. Loads and reads the specified WAV audio file.
 * 3. Loads the vocabulary and finds the blank token index.
 * 4. Preprocesses the audio data for model input.
 * 5. Runs the encoder model to obtain encoded representations.
 * 6. For each batch, decodes the encoder output using the decoder joint model:
 *    - Iteratively decodes tokens and timestamps.
 *    - Handles blank tokens and state management.
 *    - Limits the number of emitted tokens per step.
 * 7. Converts the recognized token sequence to text and prints the result.
 *
 * Command-line options:
 *   -m <model_path>   Path to the HuggingFace model directory (default: "../resources/model")
 *   -f <audio_file>   Path to the audio file (default: "../resources/audio/jfk.wav")
 *   -h                Show help message
 */
int main(int argc, char *argv[]) {
    // Please set your own model path. Download the model from: https://huggingface.co/istupakov/parakeet-tdt-0.6b-v2-onnx
    std::string model_path = "../resources/model";
    std::string audio_file = "../resources/audio/jfk.wav";
    bool show_help = false;
    auto cli = (
        clipp::option("-m").doc("Path to the HugginFace model download diretory") & clipp::value("model_path", model_path),
        clipp::option("-f").doc("Path to the audio file") & clipp::value("audio_file", audio_file),
        clipp::option("-h").set(show_help).doc("Show help")
    );
    if (!clipp::parse(argc, argv, cli) || show_help) {
        std::cout << "Usage:\n";
        std::cout << clipp::usage_lines(cli) << std::endl;
        exit(EXIT_FAILURE);
    }
    // 1. Load audio file (Requires 16000Hz, mono, s16)
    wav::WavReader wav_reader{};
    if (!wav_reader.open_file(audio_file)) {
        exit(EXIT_FAILURE);
    }
    std::vector<float> audio_wav(wav_reader.num_samples());
    for (int i = 0; i < wav_reader.num_samples(); i++) {
        audio_wav[i] = static_cast<float>(*(wav_reader.data() + i));
    }
    // 2.Load Vocab
    auto vocab = load_vocab(std::format("{}/vocab.txt", model_path));
    auto vocab_size = static_cast<int64_t>(vocab.size());
    int blank_idx = find_blank_idx("<blk>", vocab);

    // 3.Preprocess
    std::pair<Ort::Value, Ort::Value> preprocess_outputs = preprocess(std::format("{}/nemo128.onnx", model_path), audio_wav);

    // 4.Encoder
    std::pair<Ort::Value, Ort::Value> encode_outputs = encode(std::format("{}/encoder-model.onnx", model_path),  
    std::move(preprocess_outputs.first), std::move(preprocess_outputs.second));
    auto encoder_out = encode_outputs.first.GetTensorMutableData<float>();
    auto encoder_out_lens = encode_outputs.second.GetTensorMutableData<int64_t>();
    auto encoder_out_lens_shape = encode_outputs.second.GetTensorTypeAndShapeInfo().GetShape();

    // 5.Decoder
    Ort::Env env;
    Ort::SessionOptions session_options;
    auto decoder_joint = new Ort::Session(env, std::format("{}/decoder_joint-model.onnx", model_path).c_str(), session_options);
    for (size_t batch = 0; batch < encoder_out_lens_shape.size(); ++batch) {
        std::pair<Ort::Value, Ort::Value> prev_state = create_state();
        std::vector<int> tokens;
        std::vector<int> timestamps;

        int max_tokens_per_step = 10;
        int64_t encodings_len = encoder_out_lens[batch];
        int t = 0;
        int emitted_tokens = 0;
        while (t < encodings_len) {
            std::vector<float> encoding_t(1024);
            for (int i = 0; i < 1024; i++) {
                encoding_t[i] = encoder_out[batch * 1024 * encodings_len + i * encodings_len + t];
            }
            std::tuple<std::vector<float>, int, std::pair<Ort::Value, Ort::Value>> decode_outputs = decode(decoder_joint, 
                tokens, vocab_size, blank_idx, clone_state(prev_state), encoding_t);
            auto probs = std::get<0>(decode_outputs);
            auto step = std::get<1>(decode_outputs);

            int token = argmax(probs);
            if (token != blank_idx) {
                prev_state = std::move(std::get<2>(decode_outputs));
                tokens.push_back(token);
                timestamps.push_back(t);
                emitted_tokens += 1;
            }
            if (step >= 0) {
                t += step;
                emitted_tokens = 0;
            } else if (token == blank_idx || emitted_tokens == max_tokens_per_step) {
                t += 1;
                emitted_tokens = 0;
            }
        }
        // Token to Text
        std::cout << "text: " << tokens_to_text(tokens, vocab) << std::endl;
    }
    return 0;
}
