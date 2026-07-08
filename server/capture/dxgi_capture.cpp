#include "dxgi_capture.hpp"
#include "shared/util/log.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace ud {

DxgiCapture::DxgiCapture() = default;
DxgiCapture::~DxgiCapture() = default;

Result<void> DxgiCapture::init(uint32_t monitor_index) {
    UD_TRY(create_device());

    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        return Error(ErrorCode::SystemError, "Failed to create DXGIFactory1");
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    // Iterate to find the adapter containing the requested monitor
    uint32_t adapter_idx = 0;
    while (factory->EnumAdapters1(adapter_idx, &adapter) != DXGI_ERROR_NOT_FOUND) {
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        uint32_t output_idx = 0;
        
        while (adapter->EnumOutputs(output_idx, &output) != DXGI_ERROR_NOT_FOUND) {
            if (monitor_index == 0) {
                return init_duplication(output);
            }
            monitor_index--;
            output_idx++;
        }
        adapter_idx++;
    }

    return Error(ErrorCode::InvalidParameter, "Monitor index out of bounds");
}

Result<void> DxgiCapture::create_device() {
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    D3D_FEATURE_LEVEL feature_level;

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, // Required for D2D/DWrite integration and DXGI
        feature_levels, 2, D3D11_SDK_VERSION,
        &d3d_device_, &feature_level, &d3d_context_
    );

    if (FAILED(hr)) {
        return Error(ErrorCode::SystemError, "Failed to create D3D11 device");
    }
    return Result<void>();
}

Result<void> DxgiCapture::init_duplication(Microsoft::WRL::ComPtr<IDXGIOutput> output) {
    Microsoft::WRL::ComPtr<IDXGIOutput5> output5;
    if (FAILED(output.As(&output5))) {
        return Error(ErrorCode::SystemError, "IDXGIOutput5 not supported (Windows 10+ required)");
    }

    // Prefer HDR formats if available
    DXGI_FORMAT formats[] = {DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_B8G8R8A8_UNORM};
    HRESULT hr = output5->DuplicateOutput1(
        d3d_device_.Get(), 0, 2, formats, &desktop_dupl_
    );

    if (FAILED(hr)) {
        return Error(ErrorCode::SystemError, "Failed to initialize DuplicateOutput1");
    }

    DXGI_OUTDUPL_DESC desc;
    desktop_dupl_->GetDesc(&desc);
    width_ = desc.ModeDesc.Width;
    height_ = desc.ModeDesc.Height;
    format_ = desc.ModeDesc.Format;
    
    UD_LOG_INFO("capture", "DXGI Capture initialized: {}x{} Format: {}", width_, height_, static_cast<int>(format_));
    return Result<void>();
}

Result<CapturedFrame> DxgiCapture::acquire_next_frame(uint32_t timeout_ms) {
    if (!desktop_dupl_) {
        return Error(ErrorCode::InvalidParameter, "Capture not initialized");
    }

    DXGI_OUTDUPL_FRAME_INFO frame_info;
    Microsoft::WRL::ComPtr<IDXGIResource> desktop_resource;
    
    HRESULT hr = desktop_dupl_->AcquireNextFrame(timeout_ms, &frame_info, &desktop_resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return Error(ErrorCode::Timeout);
    } else if (FAILED(hr)) {
        if (hr == DXGI_ERROR_ACCESS_LOST) {
            desktop_dupl_.Reset(); // Signal caller to re-init
            return Error(ErrorCode::SystemError, "Desktop access lost (resolution change or UAC)");
        } else if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_INVALID_CALL) {
            desktop_dupl_.Reset();
            d3d_device_.Reset();
            d3d_context_.Reset();
            staging_texture_.Reset();
            return Error(ErrorCode::SystemError, "D3D11 Device Lost. Re-initialization required.");
        }
        return Error(ErrorCode::SystemError, "AcquireNextFrame failed");
    }

    if (frame_info.LastPresentTime.QuadPart == 0) {
        // Frame not updated, release immediately
        desktop_dupl_->ReleaseFrame();
        return Error(ErrorCode::Timeout);
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> acquired_texture;
    if (FAILED(desktop_resource.As(&acquired_texture))) {
        desktop_dupl_->ReleaseFrame();
        return Error(ErrorCode::SystemError, "Failed to cast resource to Texture2D");
    }

    D3D11_TEXTURE2D_DESC desc;
    acquired_texture->GetDesc(&desc);

    UD_TRY(ensure_staging_texture(desc));

    // Zero-copy: Frame stays on GPU. We just copy from the DWM resource to our isolated resource
    d3d_context_->CopyResource(staging_texture_.Get(), acquired_texture.Get());

    CapturedFrame frame;
    frame.texture = staging_texture_;
    frame.width = width_;
    frame.height = height_;
    frame.format = format_;
    frame.has_cursor_update = (frame_info.LastMouseUpdateTime > 0);

    // Extract Dirty Rects
    if (frame_info.TotalMetadataBufferSize > metadata_buffer_.size()) {
        metadata_buffer_.resize(frame_info.TotalMetadataBufferSize);
    }

    if (frame_info.TotalMetadataBufferSize > 0) {
        UINT size_required = 0;
        hr = desktop_dupl_->GetFrameDirtyRects(
            static_cast<UINT>(metadata_buffer_.size()),
            reinterpret_cast<RECT*>(metadata_buffer_.data()),
            &size_required
        );
        if (SUCCEEDED(hr)) {
            size_t num_rects = size_required / sizeof(RECT);
            auto* rects = reinterpret_cast<RECT*>(metadata_buffer_.data());
            frame.dirty_rects.assign(rects, rects + num_rects);
        }
    }

    return frame;
}

void DxgiCapture::release_frame() {
    if (desktop_dupl_) {
        desktop_dupl_->ReleaseFrame();
    }
}

Result<void> DxgiCapture::ensure_staging_texture(const D3D11_TEXTURE2D_DESC& src_desc) {
    if (staging_texture_) {
        D3D11_TEXTURE2D_DESC desc;
        staging_texture_->GetDesc(&desc);
        if (desc.Width == src_desc.Width && desc.Height == src_desc.Height && desc.Format == src_desc.Format) {
            return Result<void>(); // Reusable
        }
    }

    D3D11_TEXTURE2D_DESC desc = src_desc;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0; // GPU only
    desc.MiscFlags = 0;

    HRESULT hr = d3d_device_->CreateTexture2D(&desc, nullptr, &staging_texture_);
    if (FAILED(hr)) {
        return Error(ErrorCode::SystemError, "Failed to create staging texture");
    }

    return Result<void>();
}

} // namespace ud
