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

#include "parakeet_speech_recognizer.h"
#include "spdlog/spdlog.h"
#include <format>
#include <memory>
#include <sstream>
#include <fstream>
#include <regex>
#include <utility>
#include "utils.h"

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
    return -1;
}

ParakeetSpeechRecognizer::ParakeetSpeechRecognizer(std::string_view model_path) {
    vocab = load_vocab(std::format("{}/vocab.txt", model_path));
    vocab_size = static_cast<int64_t>(vocab.size());
    blank_idx = find_blank_idx("<blk>", vocab);
    if (blank_idx == -1) {
        spdlog::error("Token not found in vocabulary: <blk>");
        exit(EXIT_FAILURE);
    }
    preprocessor = std::make_shared<Ort::Session>(env, std::format("{}/nemo128.onnx", model_path).data(), session_options);
    encoder = std::make_shared<Ort::Session>(env, std::format("{}/encoder-model.onnx", model_path).data(), session_options);
    decoder_joint = std::make_shared<Ort::Session>(env, std::format("{}/decoder_joint-model.onnx", model_path).data(), session_options);
}

std::pair<Ort::Value, Ort::Value> ParakeetSpeechRecognizer::preprocess(const float* data_chunk, unsigned int data_chunk_nlen) {
    std::vector<float>   waveforms{data_chunk, data_chunk + data_chunk_nlen};
    std::vector<int64_t> waveforms_shape = {1, data_chunk_nlen};
    std::vector<int64_t> waveforms_lens_shape = {1};
    std::vector<int64_t> waveforms_lens = {data_chunk_nlen};

    Ort::Value input_waveforms = Ort::Value::CreateTensor<float>(memory_info, 
        waveforms.data(), waveforms.size(), waveforms_shape.data(), waveforms_shape.size());
    Ort::Value input_waveforms_lens = Ort::Value::CreateTensor<int64_t>(memory_info, 
        waveforms_lens.data(), waveforms_lens.size(), waveforms_lens_shape.data(), waveforms_lens_shape.size());
    std::array<Ort::Value, 2> input_tensors = {std::move(input_waveforms), std::move(input_waveforms_lens)};

    auto output_tensors = preprocessor->Run(Ort::RunOptions{nullptr}, 
        preprocess_input_names.data(), input_tensors.data(), preprocess_input_names.size(), 
        preprocess_output_names.data(), preprocess_output_names.size());
    return {std::move(output_tensors[0]), std::move(output_tensors[1])};
}

std::pair<Ort::Value, Ort::Value> ParakeetSpeechRecognizer::encode(Ort::Value features, Ort::Value features_lens) {
    std::array<Ort::Value, 2> input_tensors = {std::move(features), std::move(features_lens)};
    auto output_tensors = encoder->Run(
        Ort::RunOptions{nullptr},
        encoder_input_names.data(), input_tensors.data(), encoder_input_names.size(),
        encoder_output_names.data(), encoder_output_names.size());
    return {std::move(output_tensors[0]), std::move(output_tensors[1])};
}

int argmax(const std::vector<float>& v) {
    return static_cast<int>(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
}

std::tuple<std::vector<float>, int, std::pair<Ort::Value, Ort::Value>> ParakeetSpeechRecognizer::decode(std::vector<int> prev_tokens, int64_t vocab_size, int blank_idx, 
    std::pair<Ort::Value, Ort::Value> prev_state, std::vector<float> encoder_out) {
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
    std::array<Ort::Value, 5> input_tensors = {std::move(input_encoder_outputs), std::move(input_targets), std::move(input_target_length), std::move(prev_state.first), std::move(prev_state.second)};

    auto output_tensors = decoder_joint->Run(Ort::RunOptions{nullptr},
    decoder_input_names.data(), input_tensors.data(), decoder_input_names.size(),
    decoder_output_names.data(), decoder_output_names.size());

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

std::pair<Ort::Value, Ort::Value> create_state(Ort::MemoryInfo& memory_info) {
    std::vector<int64_t> state_shape = {2, 1, 640};
    Ort::Value state_1 = Ort::Value::CreateTensor<float>(
        memory_info, new float[1280]{0}, 1280, state_shape.data(), state_shape.size());
    Ort::Value state_2 = Ort::Value::CreateTensor<float>(
        memory_info, new float[1280]{0}, 1280, state_shape.data(), state_shape.size());
    return {std::move(state_1), std::move(state_2)};
}

std::pair<Ort::Value, Ort::Value> clone_state(Ort::MemoryInfo& memory_info, std::pair<Ort::Value, Ort::Value>& state) {
    std::vector<int64_t> state_shape = {2, 1, 640};
    Ort::Value state_1 = Ort::Value::CreateTensor<float>(
        memory_info, state.first.GetTensorMutableData<float>(), 1280, state_shape.data(), state_shape.size()); 
    Ort::Value state_2 = Ort::Value::CreateTensor<float>(
        memory_info, state.second.GetTensorMutableData<float>(), 1280, state_shape.data(), state_shape.size()); 
    return {std::move(state_1), std::move(state_2)};
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

bool is_sentence_end(std::string_view text) {
    if (!text.empty() && !std::ispunct(text.back())) {
        return false;
    }
    std::vector<std::string_view> suffixes = {",", "'", "-", ":", "%", "$", "/"};
    return !std::any_of(suffixes.begin(), suffixes.end(), [text](std::string_view suffix) {
        return utils::ends_with(text, suffix);
    });
}

std::pair<std::string, bool> ParakeetSpeechRecognizer::recognize_text(const float* data_chunk, unsigned int data_chunk_nlen) {
    std::pair<Ort::Value, Ort::Value> preprocess_outputs = preprocess(data_chunk, data_chunk_nlen);
    std::pair<Ort::Value, Ort::Value> encode_outputs = encode(std::move(preprocess_outputs.first), std::move(preprocess_outputs.second));
    auto encoder_out = encode_outputs.first.GetTensorMutableData<float>();
    auto encoder_out_lens = encode_outputs.second.GetTensorMutableData<int64_t>();
    auto encoder_out_lens_shape = encode_outputs.second.GetTensorTypeAndShapeInfo().GetShape();

    std::string text;
    for (size_t batch = 0; batch < encoder_out_lens_shape.size(); ++batch) {
        std::pair<Ort::Value, Ort::Value> prev_state = create_state(memory_info);
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
            std::tuple<std::vector<float>, int, std::pair<Ort::Value, Ort::Value>> decode_outputs = decode(tokens, vocab_size, blank_idx, clone_state(memory_info, prev_state), encoding_t);
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
        text = tokens_to_text(tokens, vocab);
    }
    return std::make_pair(text, is_sentence_end(text));
}