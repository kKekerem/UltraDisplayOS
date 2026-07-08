#include "shared/audio/audio_pipeline.hpp"
#include <opus.h>
#include <stdexcept>
#include <iostream>

namespace ud {

OpusEncoderWrapper::OpusEncoderWrapper() {}

OpusEncoderWrapper::~OpusEncoderWrapper() {
    if (opus_encoder_) {
        opus_encoder_destroy(static_cast<OpusEncoder*>(opus_encoder_));
    }
}

bool OpusEncoderWrapper::init(const AudioFormat& format, uint32_t bitrate_bps) {
    format_ = format;
    int err = OPUS_OK;
    OpusEncoder* encoder = opus_encoder_create(format.sample_rate, format.channels, OPUS_APPLICATION_AUDIO, &err);
    
    if (err != OPUS_OK || !encoder) {
        return false;
    }
    
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bitrate_bps));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(10));
    opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
    
    opus_encoder_ = encoder;
    return true;
}

std::vector<uint8_t> OpusEncoderWrapper::encode(std::span<const float> pcm_data) {
    if (!opus_encoder_) return {};

    int frame_size = pcm_data.size() / format_.channels;
    // max payload size for opus is generally 4000 bytes
    std::vector<uint8_t> packet(4000);
    
    int bytes = opus_encode_float(
        static_cast<OpusEncoder*>(opus_encoder_), 
        pcm_data.data(), 
        frame_size, 
        packet.data(), 
        packet.size()
    );
    
    if (bytes < 0) {
        return {};
    }
    
    packet.resize(bytes);
    return packet;
}

OpusDecoderWrapper::OpusDecoderWrapper() {}

OpusDecoderWrapper::~OpusDecoderWrapper() {
    if (opus_decoder_) {
        opus_decoder_destroy(static_cast<OpusDecoder*>(opus_decoder_));
    }
}

bool OpusDecoderWrapper::init(const AudioFormat& format) {
    format_ = format;
    int err = OPUS_OK;
    OpusDecoder* decoder = opus_decoder_create(format.sample_rate, format.channels, &err);
    
    if (err != OPUS_OK || !decoder) {
        return false;
    }
    
    opus_decoder_ = decoder;
    return true;
}

std::vector<float> OpusDecoderWrapper::decode(std::span<const uint8_t> opus_packet) {
    if (!opus_decoder_) return {};

    // 120ms is the maximum frame size for Opus
    int max_frame_size = 120 * format_.sample_rate / 1000;
    std::vector<float> pcm(max_frame_size * format_.channels);
    
    int decoded_samples = opus_decode_float(
        static_cast<OpusDecoder*>(opus_decoder_), 
        opus_packet.data(), 
        opus_packet.size(), 
        pcm.data(), 
        max_frame_size, 
        0
    );
    
    if (decoded_samples < 0) {
        return {};
    }
    
    pcm.resize(decoded_samples * format_.channels);
    return pcm;
}

} // namespace ud
