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

#include "udp_codec.h"
#include <cstring>

uint8_t* encode(Packet* packet, uint32_t* length) {
    uint32_t data_size = PACKET_HEADER_SIZE + packet->body_size;
    auto data = new uint8_t[data_size];
    int i = 0;
    data[i++] = packet->type;
    data[i++] = static_cast<uint8_t>(packet->timestamp) & 0xFF;
    data[i++] = static_cast<uint8_t>(packet->timestamp >> 8)  & 0xFF;
    data[i++] = static_cast<uint8_t>(packet->timestamp >> 16) & 0xFF;
    data[i++] = static_cast<uint8_t>(packet->timestamp >> 24) & 0xFF;
    data[i++] = static_cast<uint8_t>(packet->timestamp >> 32) & 0xFF;
    data[i++] = static_cast<uint8_t>(packet->timestamp >> 40) & 0xFF;
    data[i++] = static_cast<uint8_t>(packet->timestamp >> 48) & 0xFF;
    data[i++] = static_cast<uint8_t>(packet->timestamp >> 56) & 0xFF;
    data[i++] = packet->S;
    data[i++] = packet->E;
    data[i++] = static_cast<uint8_t>(packet->body_size) & 0xFF;
    data[i++] = static_cast<uint8_t>(packet->body_size >> 8) & 0xFF;
    memcpy(data + i, packet->body, packet->body_size);
    *length = data_size;
    return data;
}

Packet* decode(const uint8_t* data) {
    auto packet = new Packet();
    packet->type = data[0];
    packet->timestamp = static_cast<int64_t>(data[1]) | (static_cast<int64_t>(data[2]) << 8)
            | (static_cast<int64_t>(data[3]) << 16) | (static_cast<int64_t>(data[4]) << 24)
            | (static_cast<int64_t>(data[5]) << 32) | (static_cast<int64_t>(data[6]) << 40)
            | (static_cast<int64_t>(data[7]) << 48) | (static_cast<int64_t>(data[8]) << 56);
    packet->S = data[9];
    packet->E = data[10];
    packet->body_size = static_cast<int16_t>(data[11]) | (static_cast<int16_t>(data[12]) << 8);

    auto body = new uint8_t[packet->body_size];
    memcpy(body, data + PACKET_HEADER_SIZE, packet->body_size);
    packet->body = body;
    return packet;
}