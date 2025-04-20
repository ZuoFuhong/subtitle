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
#include "audio_recorder.h"
#include "lru_queue.h"
#include "convert_timer.h"
#include "offline_convert_timer.h"
#include "subtitle_window.h"
#include "../third_party/clipp.h"
#include "utils.h"

void handle_sigint(int sig) {
    if (sig == SIGINT) {
        std::cout << "Received SIGINT signal." << std::endl;
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::info);
    std::string address = "127.0.0.1:8000";
    std::string mode = "offline";
    auto cli = (
        clipp::option("-m").doc("ASR provider mode") & clipp::value("mode", mode),
        clipp::option("-s").doc("ASR server address") & clipp::value("address", address)
    );
    parse(argc, argv, cli);
    signal(SIGINT, handle_sigint);

    auto audio_queue = new LRUQueue("audio", 100);
    auto subtitle_queue = new LRUQueue("subtitle", 10);
    auto window = SubtitleWindow::new_subtitle_window(subtitle_queue);

    auto audio_recorder = AudioRecorder::new_audio_recorder(audio_queue);
    audio_recorder->turn_on();

    if (mode == "server") {
        std::string ip;
        unsigned short port;
        if (address.empty() || !utils::parse_address(address, ip, port)) {
            std::cerr << "Error: invalid ASR server address." << std::endl;
            exit(EXIT_FAILURE);
        }
        spdlog::info("ASR server target: {}", address);
        auto convert_timer = ConvertTimer::new_convert_timer(audio_queue, subtitle_queue);
        convert_timer->set_target(ip, port);
        std::thread(&ConvertTimer::start, convert_timer).detach();
    } else {
        // 离线模式
        auto convert_timer = OfflineConvertTimer::new_convert_timer(audio_queue, subtitle_queue);
        std::thread(&OfflineConvertTimer::start, convert_timer).detach();
    }

    window->run();
    return 0;
}
