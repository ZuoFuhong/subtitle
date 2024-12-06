#include "audio_codec.h"
#include "packet.h"
#include <opus/opus.h>

const int FRAME_SIZE  = 320; // 20ms at 16kHz
const int SAMPLE_RATE = 16000; // 采样率
const int CHANNELS    = 1; // 单声道

const int BIT_RATE = 32000; // 比特率, 暂不支持动态调整
const int BUFFER_SIZE = 640; // 缓冲区

AudioCodec* AudioCodec::new_audio_codec() {
    // 编码器
    int err;
    OpusEncoder *encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &err);
    if (err < 0) {
        std::cerr << "Failed to create encoder: " << opus_strerror(err) << std::endl;
        exit(-1);
    }
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(BIT_RATE));

    // 解码器
    OpusDecoder *decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
    if (err < 0) {
        std::cerr << "Failed to create decoder: " << opus_strerror(err) << std::endl;
        exit(-1);
    }
    auto codec = new AudioCodec();
    codec->encoder = encoder;
    codec->decoder = decoder;
    return codec;
}

void int16_to_uint8(const int16_t* input, uint8_t* output, size_t input_size) {
    for (size_t i = 0; i < input_size; ++i) {
        // 小端字节序
        output[i * 2] = static_cast<uint8_t>(input[i] & 0xFF);
        output[i * 2 + 1] = static_cast<uint8_t>((input[i] >> 8) & 0xFF);
    }
}

void uint8_to_int16(const uint8_t* input, int16_t* output, size_t input_size) {
    if (input_size % 2 != 0) {
        std::cerr << "Input size must be even" << std::endl;
        exit(-1);
    }
    for (size_t i = 0; i < input_size; i += 2) {
        // 小端字节序
        output[i / 2] = static_cast<int16_t>(input[i] | (input[i + 1] << 8));
    }
}

// 音频编码
Packet* AudioCodec::encode(Packet* av_packet) {
    int16_t pcm_buffer[FRAME_SIZE * CHANNELS];
    uint8_to_int16(av_packet->body, pcm_buffer, av_packet->body_size);

    auto opus_data = new uint8_t[BUFFER_SIZE];
    opus_int32 encoded_bytes_size = opus_encode(encoder, pcm_buffer, FRAME_SIZE, opus_data, BUFFER_SIZE);
    if (encoded_bytes_size < 0) {
        std::cerr << "Failed to encode: " << opus_strerror(encoded_bytes_size) << std::endl;
        exit(-1);
    }
    auto packet = new Packet();
    packet->type = av_packet->type;
    packet->timestamp = av_packet->timestamp;
    packet->body_size = encoded_bytes_size;
    packet->body = opus_data;
    return packet;
}

// 音频解码
Packet* AudioCodec::decode(Packet* opus_packet) {
    int16_t pcm_buffer[FRAME_SIZE * CHANNELS];

    int decoded_sample_size = opus_decode(decoder, opus_packet->body, opus_packet->body_size, pcm_buffer, FRAME_SIZE, 0);
    if (decoded_sample_size < 0) {
        std::cerr << "Failed to decode: " << opus_strerror(decoded_sample_size) << std::endl;
        exit(-1);
    }
    auto pcm_data_bytes = new uint8_t[decoded_sample_size * 2];
    int16_to_uint8(pcm_buffer, pcm_data_bytes, FRAME_SIZE);
    auto pcm_packet = new Packet();
    pcm_packet->type = opus_packet->type;
    pcm_packet->timestamp = opus_packet->timestamp;
    pcm_packet->body_size = decoded_sample_size * 2;
    pcm_packet->body = pcm_data_bytes;
    return pcm_packet;
}

AudioCodec::~AudioCodec() {
    opus_encoder_destroy(encoder);
    opus_decoder_destroy(decoder);
}