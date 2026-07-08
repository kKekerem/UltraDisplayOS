#pragma once

#include <cstdint>
#include <vector>
#include <span>

namespace ud {

struct AudioFormat {
    uint32_t sample_rate; // e.g. 48000, 96000, 192000
    uint8_t channels;     // e.g. 2, 6 (5.1), 8 (7.1)
    uint8_t bit_depth;    // 16, 24, 32 (Float)
};

class OpusEncoderWrapper {
public:
    OpusEncoderWrapper();
    ~OpusEncoderWrapper();

    bool init(const AudioFormat& format, uint32_t bitrate_bps);
    
    // Encodes PCM data. Returns Opus packet.
    std::vector<uint8_t> encode(std::span<const float> pcm_data);

private:
    void* opus_encoder_{nullptr};
    AudioFormat format_{};
};

class OpusDecoderWrapper {
public:
    OpusDecoderWrapper();
    ~OpusDecoderWrapper();

    bool init(const AudioFormat& format);

    // Decodes Opus packet back to PCM float data.
    std::vector<float> decode(std::span<const uint8_t> opus_packet);

private:
    void* opus_decoder_{nullptr};
    AudioFormat format_{};
};

} // namespace ud
