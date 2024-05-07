#include "convert_timer.h"
#include "udp_codec.h"
#include <spdlog/spdlog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <utility>

// 音频包 20ms
const int FRAME_DURATION = 20;

// 缓冲区-最大传输单元
const int BUFFER_SIZE = 1200;

ConvertTimer::ConvertTimer() = default;

ConvertTimer::~ConvertTimer() = default;

ConvertTimer* ConvertTimer::new_convert_timer(LRUQueue* m_audio_queue, LRUQueue* m_subtitle_queue) {
    auto timer = new ConvertTimer();
    timer->m_audio_queue = m_audio_queue;
    timer->m_subtitle_queue = m_subtitle_queue;
    return timer;
}

void ConvertTimer::set_target(std::string target) {
    m_target = std::move(target);
}

static void send_udp_packet(int sockfd, const sockaddr_in servaddr, Packet* packet) {
    uint32_t data_size = 0;
    uint8_t* data = encode(packet, &data_size);
    if (sendto(sockfd, data, data_size, 0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        spdlog::error("Subtitle convert timer sendto failed");
        exit(EXIT_FAILURE);
    }
    delete [] data;
    spdlog::debug("Subtitle convert timer sendto success packet ts: {} size: {}", packet->timestamp, packet->body_size);
}

void subtitle_result_task(int sockfd,  const sockaddr_in servaddr, LRUQueue* m_subtitle_queue) {
    auto buffer = new uint8_t[BUFFER_SIZE];
    while (true) {
        // 每 80ms 查询字幕
        std::this_thread::sleep_for(std::chrono::milliseconds(80));

        auto req = new Packet();
        req->type = SUBTITLE;
        req->body_size = 0;
        req->body = new uint8_t[0];
        send_udp_packet(sockfd, servaddr, req);
        delete req;

        int recv_size = recvfrom(sockfd, buffer, BUFFER_SIZE, MSG_DONTWAIT, nullptr, nullptr);
        if (recv_size < 0) {
            continue;
        }
        Packet* pkt = decode(buffer);
        if (pkt->type == SUBTITLE && pkt->body_size > 0) {
            m_subtitle_queue->push(pkt);
            continue;
        }
        delete pkt;
    }
}

void ConvertTimer::start() {
    if (m_target.empty()) {
        spdlog::error("ASR server target address is empty.");
        exit(EXIT_FAILURE);
    }
    spdlog::info("ASR server target: {}", m_target);
    char ip[INET_ADDRSTRLEN];
    int port;
    sscanf(m_target.c_str(), "%[^:]:%d", ip, &port);

    int sockfd;
    struct sockaddr_in servaddr{};
    memset(&servaddr, 0, sizeof(servaddr));
    if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
        spdlog::error("Error inet_pton node addr");
        exit(EXIT_FAILURE);
    }
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        spdlog::error("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    // 单独线程获取字幕
    std::thread(subtitle_result_task, sockfd, servaddr, m_subtitle_queue).detach();
    while(true) {
        if (m_audio_queue->empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DURATION));
            continue;
        }
        Packet *av_packet = m_audio_queue->pop();
        if (av_packet->type == AUDIO) {
            send_udp_packet(sockfd, servaddr, av_packet);
        }
        delete av_packet;
    }
}