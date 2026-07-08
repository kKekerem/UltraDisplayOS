#pragma once

#include <cstdint>

namespace ud {

enum class VideoCodec : uint8_t {
    Auto = 0,
    H264,
    HEVC,
    AV1,
    RawRGB
};

enum class AudioCodec : uint8_t {
    Auto = 0,
    Opus,
    RawPCM
};

enum class ColorDepth : uint8_t {
    Color8Bit = 8,
    Color10Bit = 10,
    Color12Bit = 12
};

enum class ColorSpace : uint8_t {
    BT709_SDR = 0,
    BT2020_HDR10 = 1,
    BT2020_HDR10_PLUS = 2,
    BT2020_DolbyVision = 3
};

struct EncoderCapabilities {
    uint32_t max_width;
    uint32_t max_height;
    uint32_t max_fps;
    uint32_t supported_codecs_mask; // Bitmask of VideoCodec
    bool supports_10bit;
    bool supports_12bit;
    bool supports_lossless;
};

struct DecoderCapabilities {
    uint32_t max_width;
    uint32_t max_height;
    uint32_t max_fps;
    uint32_t supported_codecs_mask;
    bool supports_10bit;
    bool supports_12bit;
    bool supports_lossless;
};

} // namespace ud
