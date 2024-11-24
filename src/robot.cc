#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <utility>
#include <thread>
#include <spdlog/spdlog.h>
#include "packet.h"
#include "udp_codec.h"
#include "utils.h"
#include "../third_party/clipp.h"

// 采样数 20ms 音频
const int FRAME_SIZE = 320;

const int FRAME_BYTE_SIZE = FRAME_SIZE * 2;

static void send_udp_packet(std::string& ip, unsigned short port, Packet* packet) {
    int sockfd = 0;
    struct sockaddr_in servaddr{};
    memset(&servaddr, 0, sizeof(servaddr));
    if (inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr) <= 0) {
        spdlog::error("Error inet_pton node addr");
        exit(EXIT_FAILURE);
    }
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        spdlog::error("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    uint32_t data_size = 0;
    uint8_t* data = encode(packet, &data_size);
    if (sendto(sockfd, data, data_size, 0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        spdlog::error("Robot sendto failed");
        exit(EXIT_FAILURE);
    }
    delete [] data;
    spdlog::info("Robot sendto success packet ts: {} size: {}", packet->timestamp, packet->body_size);
}

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::info);
    std::string file = "../resources/audio/jfk.wav";
    std::string address = "127.0.0.1:8000";
    auto cli = (
        clipp::option("-f").doc("file") & clipp::value("file", file),
        clipp::option("-s").doc("ASR server address") & clipp::value("address", address)
    );
    parse(argc, argv, cli);
    std::string ip;
    unsigned short port;
    if (address.empty() || !utils::parse_address(address, ip, port)) {
        std::cerr << "Error: invalid ASR server address." << std::endl;
        exit(EXIT_FAILURE);
    }
    if (file.empty()) {
        std::cerr << "Error: file cannot be empty" << std::endl;
        exit(1);
    }
    // 读取文件
    std::ifstream fin(file, std::ios::in | std::ios::binary);
    if (fin.fail()) {
        std::cerr << "Error: file does not exist" << std::endl;
        exit(1);
    }
    fin.seekg(44, std::ios::beg);
    short seg;
    std::vector<short> stream;
    while (fin.read(reinterpret_cast<char *>(&seg), sizeof(seg))) {
        stream.push_back(seg);
    }
    int step = FRAME_SIZE;
    for (unsigned i = 0; i < stream.size(); i += step) {
        auto stream_seg = (uint8_t *)(&*stream.begin() + i);

        auto pkt = new Packet();
        pkt->type = AUDIO;
        pkt->timestamp = utils::current_timestamp();
        pkt->body_size = FRAME_BYTE_SIZE;
        pkt->body = stream_seg;
        send_udp_packet(ip, port, pkt);

        // 20ms 音频发一次
        // 模拟说话节奏
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    spdlog::info("done");
}
