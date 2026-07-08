#pragma once

#include "encoder.hpp"

namespace ud {

// Forward declaration to avoid pulling in the massive nvEncodeAPI.h in headers
class NvencImpl;

class NvencEncoder : public IEncoder {
public:
    NvencEncoder();
    ~NvencEncoder() override;

    Result<void> init(Microsoft::WRL::ComPtr<ID3D11Device> d3d_device, const EncoderConfig& config) override;
    Result<void> reconfigure(const EncoderConfig& config) override;
    Result<std::vector<EncodedPacket>> encode(Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture) override;

private:
    std::unique_ptr<NvencImpl> impl_;
};

} // namespace ud
