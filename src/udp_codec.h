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