#pragma once

#include "shared/util/result.hpp"


#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace ud {

class D3D11ColorConverter {
public:
    D3D11ColorConverter();
    ~D3D11ColorConverter();

    Result<void> init(Microsoft::WRL::ComPtr<ID3D11Device> d3d_device);

    // Converts the captured BGRA/RGBA16F texture into an NV12 or P010 texture suitable for the encoder.
    // The operation is performed entirely on the D3D11 Video Processor.
    Result<Microsoft::WRL::ComPtr<ID3D11Texture2D>> convert(Microsoft::WRL::ComPtr<ID3D11Texture2D> src_texture, bool request_hdr);

private:
    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d_context_;
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
    Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> video_enum_;

    // Cached output textures to avoid reallocation
    Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> nv12_output_view_;
    
    Microsoft::WRL::ComPtr<ID3D11Texture2D> p010_texture_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> p010_output_view_;

    uint32_t current_width_{0};
    uint32_t current_height_{0};

    Result<void> ensure_output_texture(uint32_t width, uint32_t height, bool is_hdr);
};

} // namespace ud
