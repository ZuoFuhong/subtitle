#include <iostream>
#include <spdlog/spdlog.h>
#include "spdlog/sinks/stdout_color_sinks.h"
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

    auto audio_queue = new LRUQueue("audio", 50);
    auto subtitle_queue = new LRUQueue("subtitle", 10);
    auto window = SubtitleWindow::new_subtitle_window(subtitle_queue);

    // 音频采集
    auto audio_recorder = AudioRecorder::new_audio_recorder(audio_queue);
    audio_recorder->turn_on();

    // 语音识别
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

    // 字幕上屏
    window->run();
    return 0;
}
