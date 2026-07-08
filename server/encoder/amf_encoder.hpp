#pragma once

#include "encoder.hpp"
#include <memory>

namespace ud {

class AmfImpl;

class AmfEncoder : public IEncoder {
public:
    AmfEncoder();
    ~AmfEncoder() override;

    Result<void> init(Microsoft::WRL::ComPtr<ID3D11Device> d3d_device, const EncoderConfig& config) override;
    Result<void> reconfigure(const EncoderConfig& config) override;
    Result<std::vector<EncodedPacket>> encode(Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture) override;

private:
    std::unique_ptr<AmfImpl> impl_;
};

} // namespace ud
