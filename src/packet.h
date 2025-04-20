#pragma once

#include <cstdint>

const int AUDIO    = 1;
const int VIDEO    = 2;
const int SUBTITLE = 3;

struct Packet {
    uint8_t  type; // 数据类型: 1-音频 2-视频
    int64_t  timestamp; // 时间戳, 单位: ms
    uint8_t  S; // 分片第一个包
    uint8_t  E; // 分片最后一个
    uint16_t body_size; // 数据长度
    uint8_t* body; // 数据 Body

    Packet(): type(0), timestamp(0), S(0), E(0), body_size(0), body(nullptr) {}

    ~Packet() {
        delete [] body;
    }
};