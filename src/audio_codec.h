#pragma once

#include "packet.h"
#include <opus/opus.h>

class AudioCodec {
public:
    AudioCodec() = default;
    ~AudioCodec();

    static AudioCodec* new_audio_codec();

    Packet* encode(Packet* av_packet);

    Packet* decode(Packet* opus_packet);

private:
    OpusEncoder* encoder{};

    OpusDecoder* decoder{};
};