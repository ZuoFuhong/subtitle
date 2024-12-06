/**
 * 语音活动检测使用 Silero-vad ONNX 预训练模型
 *
 * Code lifted from https://github.com/snakers4/silero-vad/blob/master/examples/cpp/silero-vad-onnx.cpp
 * Python code lifted from https://github.com/snakers4/silero-vad/blob/master/src/silero_vad/utils_vad.py
 */
#include <vector>
#include <iostream>
#include <cstring>
#include <memory>
#include "silero_vad.h"

VadIterator::VadIterator(const std::string& model_path,
                         int freq, int windows_frame_size,
                         float Threshold, int min_silence_duration_ms,
                         int speech_pad_ms, int min_speech_duration_ms,
                         int max_speech_duration_s) {
    init_onnx_model(model_path);
    threshold = Threshold;
    sample_rate = freq;
    sr_per_ms = sample_rate / 1000;

    window_size_samples = windows_frame_size * sr_per_ms;

    min_speech_samples = sr_per_ms * min_speech_duration_ms;
    speech_pad_samples = sr_per_ms * speech_pad_ms;

    max_speech_samples = sample_rate * max_speech_duration_s - window_size_samples - 2 * speech_pad_samples;
    min_silence_samples = sr_per_ms * min_silence_duration_ms;
    min_silence_samples_at_max_speech = sr_per_ms * 98;

    input.resize(window_size_samples);
    input_node_dims[0] = 1;
    input_node_dims[1] = window_size_samples;

    _state.resize(size_state);
    sr.resize(1);
    sr[0] = sample_rate;
}

void VadIterator::init_engine_threads(int inter_threads, int intra_threads) {
    // The method should be called in each thread/proc in multi-thread/proc work
    session_options.SetIntraOpNumThreads(intra_threads);
    session_options.SetInterOpNumThreads(inter_threads);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    session_options.SetLogSeverityLevel(OrtLoggingLevel::ORT_LOGGING_LEVEL_ERROR);
};

void VadIterator::init_onnx_model(const std::string& model_path) {
    // Init threads = 1 for
    init_engine_threads(1, 1);
    // Load model
    session = std::make_shared<Ort::Session>(env, model_path.c_str(), session_options);
}

void VadIterator::reset_states() {
    std::memset(_state.data(), 0.0f, _state.size() * sizeof(float));
    triggered = false;
    temp_end = 0;
    current_sample = 0;
    prev_end = next_start = 0;
    speeches.clear();
    current_speech = AudioClip{};
}

void VadIterator::predict(const std::vector<float> &data) {
    // Infer
    // Create ort tensors
    input.assign(data.begin(), data.end());
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
    ort_outputs = session->Run(
            Ort::RunOptions{nullptr},
            input_node_names.data(), ort_inputs.data(), ort_inputs.size(),
            output_node_names.data(), output_node_names.size());

    // Output probability & update h,c recursively
    float speech_prob = ort_outputs[0].GetTensorMutableData<float>()[0];
    auto stateN = ort_outputs[1].GetTensorMutableData<float>();
    std::memcpy(_state.data(), stateN, size_state * sizeof(float));

    // Push forward sample index
    current_sample += window_size_samples;

    // Reset temp_end when > threshold
    if ((speech_prob >= threshold)) {
        if (temp_end != 0) {
            temp_end = 0;
            if (next_start < prev_end) {
                next_start = current_sample - window_size_samples;
            }
        }
        if (!triggered) {
            triggered = true;
            current_speech.start = current_sample - window_size_samples;
        }
        return;
    }

    if (triggered && ((current_sample - current_speech.start) > max_speech_samples)) {
        if (prev_end > 0) {
            current_speech.end = prev_end;
            speeches.push_back(current_speech);
            current_speech = AudioClip{};

            // previously reached silence(< neg_thres) and is still not speech(< thres)
            if (next_start < prev_end) {
                triggered = false;
            } else {
                current_speech.start = next_start;
            }
            prev_end = 0;
            next_start = 0;
            temp_end = 0;
        } else {
            current_speech.end = current_sample;
            speeches.push_back(current_speech);
            current_speech = AudioClip{};
            prev_end = 0;
            next_start = 0;
            temp_end = 0;
            triggered = false;
        }
        return;
    }

    if ((speech_prob >= (threshold - 0.15)) && (speech_prob < threshold)) {
        return;
    }

    // 4) End
    if (speech_prob < (threshold - 0.15)) {
        if (triggered) {
            if (temp_end == 0) {
                temp_end = current_sample;
            }
            if (current_sample - temp_end > min_silence_samples_at_max_speech) {
                prev_end = temp_end;
            }
            if ((current_sample - temp_end) < min_silence_samples) {
                // a. silence < min_slience_samples, continue speaking
            } else {
                // b. silence >= min_slience_samples, end speaking
                current_speech.end = temp_end;
                if (current_speech.end - current_speech.start > min_speech_samples) {
                    speeches.push_back(current_speech);
                    current_speech = AudioClip{};
                    prev_end = 0;
                    next_start = 0;
                    temp_end = 0;
                    triggered = false;
                }
            }
        } else {
            // may first window see end state.
        }
        return;
    }
}

void VadIterator::process(const std::vector<float>& input_wav) {
    reset_states();
    audio_length_samples = static_cast<int>(input_wav.size());
    for (int j = 0; j < audio_length_samples; j += window_size_samples) {
        if (j + window_size_samples > audio_length_samples) {
            break;
        }
        std::vector<float> r{ &input_wav[0] + j, &input_wav[0] + j + window_size_samples };
        predict(r);
    }
}

std::vector<AudioClip> VadIterator::get_speech_timestamps() const{
    return speeches;
}