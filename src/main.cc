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

#include <iostream>
#include <spdlog/spdlog.h>
#include <filesystem>
#include "audio_recorder.h"
#include "lru_queue.h"
#include "convert_timer.h"
#include "offline_convert_timer.h"
#include "subtitle_window.h"
#include "../third_party/clipp.h"
#include "utils.h"

void handle_sigint(int sig) {
    if (sig == SIGINT) {
        spdlog::info("Received SIGINT signal.");
        exit(0);
    }
}

const std::string GGMl_SMALL_EN = "ggml-small.en.bin";

const std::string GGMl_MEDIUM_EN = "ggml-medium.en.bin";

void prepare_model_file(std::string_view model_name) {
    if (model_name != GGMl_SMALL_EN && model_name != GGMl_MEDIUM_EN) {
        spdlog::warn("Unsupported model `{}`.", model_name);
        spdlog::info("You can use the '-f' option to specify the model name. For example: -f {} or {}", GGMl_SMALL_EN, GGMl_MEDIUM_EN);
        exit(EXIT_FAILURE);
    }
    std::string model_path = "../resources/model/" + std::string(model_name);
    if (!std::filesystem::exists(model_path)) {
        std::string download_url = fmt::format("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/{}", model_name);
        spdlog::info("Downloading model file from {} to {}", download_url, model_path);
        if (!utils::curl_download(download_url, model_path, "10M")) {
            spdlog::error("Failed to download `{}` model.", model_name);
            spdlog::info("Please download the model file manually and place it in the resources/model directory.");
            exit(EXIT_FAILURE);
        }
    }
    std::string vad_model = "../resources/model/silero_vad.onnx";
    if (!std::filesystem::exists(vad_model)) {
        spdlog::error("Not found `silero_vad.onnx` model file.");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::info);
    std::string address = "127.0.0.1:8000";
    std::string mode = "offline";
    std::string model_name = GGMl_SMALL_EN;
    bool show_help = false;
    auto cli = (
        clipp::option("-m").doc("ASR provider mode") & clipp::value("mode", mode),
        clipp::option("-s").doc("ASR server address") & clipp::value("address", address),
        clipp::option("-f").doc("ASR model name") & clipp::value("model_name", model_name),
        clipp::option("-h").set(show_help).doc("Show help")
    );
    if (!clipp::parse(argc, argv, cli) || show_help) {
        std::cout << "Usage:\n";
        std::cout << clipp::usage_lines(cli) << std::endl;
        exit(0);
    }
    signal(SIGINT, handle_sigint);
    prepare_model_file(model_name);

    auto audio_queue = new LRUQueue("audio", 100);
    auto subtitle_queue = new LRUQueue("subtitle", 10);
    auto window = SubtitleWindow::new_subtitle_window(subtitle_queue);

    auto audio_recorder = AudioRecorder::new_audio_recorder(audio_queue);
    audio_recorder->turn_on();

    if (mode == "server") {
        std::string ip;
        unsigned short port;
        if (address.empty() || !utils::parse_address(address, ip, port)) {
            spdlog::error("Invalid ASR server address: {}", address);
            exit(EXIT_FAILURE);
        }
        spdlog::info("ASR server target: {}", address);
        auto convert_timer = ConvertTimer::new_convert_timer(audio_queue, subtitle_queue);
        convert_timer->set_target(ip, port);
        std::thread(&ConvertTimer::start, convert_timer).detach();
    } else {
        // 离线模式
        spdlog::info("ASR offline mode with `{}` model.", model_name);
        auto convert_timer = OfflineConvertTimer::new_convert_timer(audio_queue, subtitle_queue);
        std::thread(&OfflineConvertTimer::start, convert_timer).detach();
    }

    window->run();
    return 0;
}
