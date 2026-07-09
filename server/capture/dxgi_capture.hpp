#pragma once

#include "shared/util/result.hpp"
#include <vector>
#include <memory>


#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace ud {

struct CapturedFrame {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    std::vector<RECT> dirty_rects;
    std::vector<DXGI_OUTDUPL_MOVE_RECT> move_rects;
    uint32_t width;
    uint32_t height;
    DXGI_FORMAT format;
    bool has_cursor_update;
};

class DxgiCapture {
public:
    DxgiCapture();
    ~DxgiCapture();

    // Initialize capture on the specified monitor index
    Result<void> init(uint32_t monitor_index = 0);

    // Blocks until a new frame is available or timeout occurs
    // Returns ErrorCode::Timeout if no frame is ready (not a critical failure)
    Result<CapturedFrame> acquire_next_frame(uint32_t timeout_ms);

    // Must be called after processing the CapturedFrame
    void release_frame();

    Microsoft::WRL::ComPtr<ID3D11Device> device() const { return d3d_device_; }
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context() const { return d3d_context_; }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d_context_;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> desktop_dupl_;
    
    // Staging texture for zero-copy D3D11 operations
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;
    
    std::vector<uint8_t> metadata_buffer_;
    uint32_t width_{0};
    uint32_t height_{0};
    DXGI_FORMAT format_{DXGI_FORMAT_UNKNOWN};

    Result<void> create_device();
    Result<void> init_duplication(Microsoft::WRL::ComPtr<IDXGIOutput> output);
    Result<void> ensure_staging_texture(const D3D11_TEXTURE2D_DESC& desc);
};

} // namespace ud
