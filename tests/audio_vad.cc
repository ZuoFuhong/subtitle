#include <iostream>
#include "../third_party/wav.h"
#include "../src/asrapi.h"

int main() {
    // 归一化预处理
    wav::WavReader wav_reader{};
    if (!wav_reader.open_file("../resources/audio/jfk.wav")) { // 16000, 1, s16
        exit(EXIT_FAILURE);
    }
    std::vector<float> audio_wav(wav_reader.num_samples());
    for (int i = 0; i < wav_reader.num_samples(); i++) {
        audio_wav[i] = static_cast<float>(*(wav_reader.data() + i));
    }

    // 新建 Session
    HANDLE session;
    auto code = ASR_create_session(session);
    if (code != ASRCode::ERROR_OK) {
        std::cerr << "ASR_create_session err code=" << code << std::endl;
        exit(EXIT_FAILURE);
    }
    size_t cursor = 0; // 游标
    int nstate = 0; // 0-没说话 1-在说话
    int nstatelast = 0;
    size_t window_size_samples = 512; // 窗口
    while (cursor + window_size_samples < audio_wav.size()) {
        code = ASR_push_buffer(session, audio_wav.data() + cursor,  window_size_samples);
        if (code != ASRCode::ERROR_OK) {
            std::cerr << "ASR_push_buffer err code=" << code << std::endl;
            exit(EXIT_FAILURE);
        }
        code = ASR_get_vad_state(session, &nstate);
        if (code != ASRCode::ERROR_OK) {
            std::cerr << "ASR_get_vad_state err code=" << code << std::endl;
            exit(EXIT_FAILURE);
        }
        std::string result;
        if (!nstatelast && nstate) {
            // 之前没说话, 现在说话
            // 新起一句
            code = ASR_begin_session(session);
            if (code != ASRCode::ERROR_OK) {
                std::cerr << "ASR_begin_session err code=" << code << std::endl;
                exit(EXIT_FAILURE);
            }
        } else if (nstatelast && !nstate) {
            // 之前在说话, 现在没说话
            code = ASR_end_session(session);
            if (code != ERROR_OK) {
                std::cerr << "ASR_end_session err code=" << code << std::endl;
                exit(EXIT_FAILURE);
            }
            // 获取稳态句子
            code = ASR_get_result(session, result);
            if (code != ERROR_OK) {
                std::cout << "ASR_get_result err code=" << code << std::endl;
                exit(EXIT_FAILURE);
            }
            if (!result.empty()) {
                std::cout << result << std::endl;
            }
        }
        nstatelast = nstate;
        cursor+=window_size_samples;
    }
    return 0;
}