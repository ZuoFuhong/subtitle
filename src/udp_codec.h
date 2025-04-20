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

#pragma once

#include "packet.h"

/**
 * 私有协议: 用于传输音视频媒体数据
 * ----------------------------------------------------------------------------
 * | PacketType(8) | Timestamp(64) | S(8) | E(8) | PacketSize(16) |   Body    |
 * ----------------------------------------------------------------------------
 */

// 最大传输单元
const int PACKET_MAX_UNIT_SIZE = 1200;

// 头部固定长度
const uint32_t PACKET_HEADER_SIZE = 13;

// 将 Packet 编码成字节数组
uint8_t* encode(Packet* packet, uint32_t* length);

// 从字节数组解析成 Packet 实例
Packet* decode(const uint8_t* data);