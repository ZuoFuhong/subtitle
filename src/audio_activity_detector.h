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

#pragma once

#include <memory>
#include <onnxruntime_cxx_api.h>

class AudioActivityDetector {
public:
    explicit AudioActivityDetector(std::string_view model_path);

    ~AudioActivityDetector() = default;

    void init_onnx_model(std::string_view model_path);

    float predict(const float* data_chunk, unsigned int nlen);

private:
    Ort::Env env;
    Ort::SessionOptions session_options;
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeCPU);

    std::shared_ptr<Ort::Session> session;

    std::vector<int64_t> input_node_dims = {1, 512 + 64};

    std::vector<int64_t> state_node_dims = {2, 1, 128};

    int64_t sr_node_dims[1] = {1};

    unsigned int state_size = 2 * 1 * 128;

    std::vector<const char*> input_names = {"input", "state", "sr"};

    std::vector<const char*> output_names = {"output", "stateN"};

    std::vector<float> input;

    std::vector<float> _state;

    std::vector<int64_t> sr;

private:
    std::vector<float> _context;

    const int context_samples = 64;  // For 16kHz, 64 samples are added as context.

    int effective_window_size = 512 + 64; // 16000Hz * 0.032s = 512 samples
};
