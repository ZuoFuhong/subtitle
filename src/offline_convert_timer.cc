#include <thread>
#include "offline_convert_timer.h"
#include "asrapi.h"
#include "utils.h"

// 音频包 20ms
const int FRAME_DURATION = 32;

// 语音活动窗口
const int WINDOW_SIZE_SAMPLES = 512;

OfflineConvertTimer* OfflineConvertTimer::new_convert_timer(LRUQueue* audio_queue, LRUQueue* subtitle_queue) {
    auto timer = new OfflineConvertTimer();
    timer->m_audio_queue = audio_queue;
    timer->m_subtitle_queue = subtitle_queue;
    return timer;
}

void OfflineConvertTimer::start() {
    HANDLE session;
    auto code = ASR_create_session(session);
    if (code != ASRCode::ERROR_OK) {
        std::cerr << "ASR_create_session err code=" << code << std::endl;
        exit(EXIT_FAILURE);
    }
    int nstate = 0; // 0-没说话 1-在说话
    int nstatelast = 0;
    std::vector<float> samples_buffer;
    while(true) {
        if (m_audio_queue->empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DURATION));
            continue;
        }
        // 归一化处理
        Packet* audio_pkt = m_audio_queue->pop();
        auto body_ptr = reinterpret_cast<int16_t*>(audio_pkt->body);
        for (int i = 0; i < audio_pkt->body_size / sizeof(int16_t); i++) {
            auto sample = static_cast<float>(body_ptr[i]) / 32768;
            samples_buffer.push_back(sample);
        }
        delete audio_pkt;
        if (samples_buffer.size() < WINDOW_SIZE_SAMPLES) {
            continue;
        }
        // 语音识别
        code = ASR_push_buffer(session, samples_buffer.data(),  WINDOW_SIZE_SAMPLES);
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
                auto object = nlohmann::json::parse(result);
                if (object.contains("text")) {
                    std::string text = object["text"];
                    size_t str_size = text.size();
                    auto body = new uint8_t[str_size];
                    std::memcpy(body, text.data(), str_size);
                    auto pkt = new Packet();
                    pkt->type = SUBTITLE;
                    pkt->timestamp = utils::current_timestamp();
                    pkt->body = body;
                    pkt->body_size = str_size;
                    m_subtitle_queue->push(pkt);
                }
            }
        }
        nstatelast = nstate;
        samples_buffer.erase(samples_buffer.begin(), samples_buffer.begin() + WINDOW_SIZE_SAMPLES);
    }
}