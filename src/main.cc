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

#include <cstdlib>
#include <iostream>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <string_view>
#include "audio_recorder.h"
#include "lru_queue.h"
#include "convert_timer.h"
#include "offline_convert_timer.h"
#include "subtitle_window.h"
#include "constants.h"
#include "../third_party/clipp.h"
#include "utils.h"

void handle_sigint(int sig) {
    if (sig == SIGINT) {
        spdlog::info("Received SIGINT signal.");
        exit(0);
    }
}

void prepare_model_file(std::string_view model_path, std::string_view model_name) {
    if (model_name != GGMl_SMALL_EN && model_name != GGMl_MEDIUM_EN && model_name != PARAKEET_TDT_0_6B_V2) {
        spdlog::warn("Unsupported model `{}`.", model_name);
        spdlog::info("You can use the '-f' option to specify the model name. For example: -f {}/{}/{}", GGMl_SMALL_EN, GGMl_MEDIUM_EN, PARAKEET_TDT_0_6B_V2);
        exit(EXIT_FAILURE);
    }
    std::string vad_model_path = std::format("{}/silero_vad.onnx", model_path);
    if (!std::filesystem::exists(vad_model_path)) {
        std::string download_url = "https://raw.githubusercontent.com/ZuoFuhong/subtitle/refs/heads/master/resources/model/silero_vad.onnx";
        if (!utils::curl_download(download_url, vad_model_path, "10M")) {
            spdlog::error("Failed to download `{}` model.", vad_model_path);
            exit(EXIT_FAILURE);
        }
         spdlog::info("Successfully downloaded `{}`.", vad_model_path);
    }
    if (model_name == PARAKEET_TDT_0_6B_V2) {
        std::vector<std::string> files = {"nemo128.onnx", "vocab.txt", "encoder-model.onnx", "decoder_joint-model.onnx", "encoder-model.onnx.data"};
        for (const auto& filename : files) {
            std::string file_path = std::format("{}/{}/{}", model_path, model_name, filename);
            if (!std::filesystem::exists(file_path)) {
                std::string download_url = fmt::format("https://huggingface.co/istupakov/parakeet-tdt-0.6b-v2-onnx/resolve/main/{}", filename);
                if (filename == "nemo128.onnx") {
                    download_url = "https://raw.githubusercontent.com/ZuoFuhong/subtitle/refs/heads/master/resources/model/parakeet-tdt-0.6b-v2/nemo128.onnx";
                }
                if (!utils::curl_download(download_url, file_path, "10M")) {
                    spdlog::error("Failed to download `{}` model.", file_path);
                    exit(EXIT_FAILURE);
                }
                spdlog::info("Successfully downloaded `{}`.", file_path);
            }
        }
    }
    if (model_name == GGMl_SMALL_EN || model_name == GGMl_MEDIUM_EN) {
        std::string file_path = std::format("{}/{}", model_path, model_name);
        if (!std::filesystem::exists(file_path)) {
            std::string download_url = fmt::format("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/{}", model_name);
            spdlog::info("Downloading model file from {} to {}", download_url, file_path);
            if (!utils::curl_download(download_url, file_path, "10M")) {
                spdlog::error("Failed to download `{}` model.", file_path);
                exit(EXIT_FAILURE);
            }
            spdlog::info("Successfully downloaded `{}`.", file_path);
        }
    }
}

std::string prepare_audio_device() {
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize! Get_Error: " << SDL_GetError() << std::endl;
        exit(EXIT_FAILURE);
    }
    int numDevices = SDL_GetNumAudioDevices(1); // Audio input devices
    if (numDevices == 0) {
        spdlog::error("No audio devices found.");
        exit(EXIT_FAILURE);
    }
    std::cout << "Available audio devices:" << std::endl;
    for (int i = 0; i < numDevices; ++i) {
        const char* deviceName = SDL_GetAudioDeviceName(i, 1);
        std::cout << i << ": " << deviceName << std::endl;
    }
    int selected = -1;
    std::cout << "Please select an audio device by number: ";
    std::cin >> selected;
    if (selected < 0 || selected >= numDevices) {
        spdlog::error("Invalid device number selected.");
        exit(EXIT_FAILURE);
    }
    const char* device_name = SDL_GetAudioDeviceName(selected, 1);
    std::cout << "You selected device: " << device_name << std::endl;
    return device_name;
}

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::info);
    std::string address = "127.0.0.1:8000";
    std::string mode = "offline";
    std::string model_path = "./model";
    std::string model_name = GGMl_SMALL_EN;
    std::string llm_model_name = "deepseek-chat";
    bool show_help = false;
    auto cli = (
        clipp::option("-mode").doc("ASR provider mode") & clipp::value("mode", mode),
        clipp::option("-s").doc("ASR server address") & clipp::value("address", address),
        clipp::option("-f").doc("ASR model path") & clipp::value("model_path", model_path),
        clipp::option("-m").doc("ASR model name") & clipp::value("model_name", model_name),
        clipp::option("-llm").doc("LLM model name") & clipp::value("llm_model_name", llm_model_name),
        clipp::option("-h").set(show_help).doc("Show help")
    );
    if (!clipp::parse(argc, argv, cli) || show_help) {
        std::cout << "Usage:\n";
        std::cout << clipp::usage_lines(cli) << std::endl;
        exit(0);
    }
    signal(SIGINT, handle_sigint);
    prepare_model_file(model_path, model_name);
    std::string audio_device_name = prepare_audio_device();

    auto audio_queue = new LRUQueue("audio", 200);
    auto subtitle_queue = new LRUQueue("subtitle", 10);
    auto window = new SubtitleWindow(subtitle_queue, llm_model_name);

    auto audio_recorder = new AudioRecorder(audio_queue, audio_device_name);
    audio_recorder->turn_on();

    if (mode == "server") {
        std::string ip;
        unsigned short port;
        if (address.empty() || !utils::parse_address(address, ip, port)) {
            spdlog::error("Invalid ASR server address: {}", address);
            exit(EXIT_FAILURE);
        }
        spdlog::info("ASR server target: {}", address);
        auto convert_timer = new ConvertTimer(audio_queue, subtitle_queue);
        convert_timer->set_target(ip, port);
        std::thread(&ConvertTimer::start, convert_timer).detach();
    } else {
        spdlog::info("ASR offline mode with `{}/{}` model.", model_path, model_name);
        auto convert_timer = new OfflineConvertTimer(audio_queue, subtitle_queue, model_path, model_name);
        std::thread(&OfflineConvertTimer::start, convert_timer).detach();
    }

    window->run();
    return 0;
}
