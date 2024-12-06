#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <utility>
#include <thread>
#include "../src/packet.h"
#include "../src/audio_codec.h"
#include "../src/udp_codec.h"
#include "../src/utils.h"
#include "../third_party/clipp.h"

// 采样数 20ms 音频
const int FRAME_SIZE = 320;

static void send_udp_packet(std::string& ip, unsigned short port, Packet* packet) {
    int sockfd = 0;
    struct sockaddr_in servaddr{};
    memset(&servaddr, 0, sizeof(servaddr));
    if (inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr) <= 0) {
        std::cerr << "Error inet_pton node addr" << std::endl;
        exit(EXIT_FAILURE);
    }
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    uint32_t data_size = 0;
    uint8_t* data = encode(packet, &data_size);
    if (sendto(sockfd, data, data_size, 0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        std::cerr << "Robot sendto failed" << std::endl;
        exit(EXIT_FAILURE);
    }
    delete [] data;
}

int main(int argc, char *argv[]) {
    std::string file = "../resources/audio/jfk.wav";
    std::string address = "9.135.97.184:8000";
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
        exit(EXIT_FAILURE);
    }
    // 读取文件
    std::ifstream fin(file, std::ios::in | std::ios::binary);
    if (fin.fail()) {
        std::cerr << "Error: file does not exist" << std::endl;
        exit(EXIT_FAILURE);
    }
    fin.seekg(44, std::ios::beg);
    short seg;
    std::vector<short> stream;
    while (fin.read(reinterpret_cast<char *>(&seg), sizeof(seg))) {
        stream.push_back(seg);
    }
    auto audio_codec = AudioCodec::new_audio_codec();
    int step = FRAME_SIZE;
    for (unsigned i = 0; i < stream.size(); i += step) {
        auto stream_seg = (uint8_t *)(&*stream.begin() + i);

        auto pkt = new Packet();
        pkt->type = AUDIO;
        pkt->timestamp = utils::current_timestamp();
        pkt->body_size = step * 2; // 一帧两个字节
        pkt->body = stream_seg;

        // 编码发送
        auto opus_pkt = audio_codec->encode(pkt);
        send_udp_packet(ip, port, opus_pkt);
        delete opus_pkt;

        // 20ms 音频发一次
        // 模拟说话节奏
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::cout << "done!" << std::endl;
}
