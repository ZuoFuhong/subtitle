#pragma once

#include "packet.h"
#include <iostream>
#include <utility>
#include <opus/opus.h>

class AudioCodec {
public:
    AudioCodec() = default;
    ~AudioCodec();

    static AudioCodec* new_audio_codec();

    // 音频编码
    Packet* encode(Packet* av_packet);

    // 音频解码
    Packet* decode(Packet* opus_packet);

private:
    // 编码器
    OpusEncoder* encoder{};

    // 解码器
    OpusDecoder* decoder{};
};