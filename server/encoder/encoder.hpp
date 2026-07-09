#pragma once

#include "shared/util/result.hpp"
#include "shared/protocol/messages.hpp"
#include "adaptive_controller.hpp"


#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <span>
#include <vector>

namespace ud {

struct EncodedPacket {
    std::vector<uint8_t> data;
    bool is_keyframe;
    uint16_t frame_number;
};

class IEncoder {
public:
    virtual ~IEncoder() = default;

    // Initialize the encoder sharing the D3D11 device used by capture to ensure zero-copy
    virtual Result<void> init(Microsoft::WRL::ComPtr<ID3D11Device> d3d_device, const EncoderConfig& config) = 0;

    // Reconfigure without re-initializing (e.g. bitrate change, force IDR)
    virtual Result<void> reconfigure(const EncoderConfig& config) = 0;

    // Encode a frame. The input must be a D3D11Texture2D (NV12 or P010) already converted by the VideoProcessor.
    // Returns a list of encoded NAL units or packets ready for transport.
    virtual Result<std::vector<EncodedPacket>> encode(Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture) = 0;
};

} // namespace ud
