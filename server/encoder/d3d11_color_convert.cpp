#include "d3d11_color_convert.hpp"
#include "shared/util/log.hpp"

namespace ud {

D3D11ColorConverter::D3D11ColorConverter() = default;
D3D11ColorConverter::~D3D11ColorConverter() = default;

Result<void> D3D11ColorConverter::init(Microsoft::WRL::ComPtr<ID3D11Device> d3d_device) {
    d3d_device_ = d3d_device;
    d3d_device_->GetImmediateContext(&d3d_context_);

    HRESULT hr = d3d_device_.As(&video_device_);
    if (FAILED(hr)) {
        return Error(ErrorCode::SystemError, "Failed to query ID3D11VideoDevice from D3D11 device");
    }

    hr = d3d_context_.As(&video_context_);
    if (FAILED(hr)) {
        return Error(ErrorCode::SystemError, "Failed to query ID3D11VideoContext from immediate context");
    }

    return Result<void>();
}

Result<void> D3D11ColorConverter::ensure_output_texture(uint32_t width, uint32_t height, bool is_hdr) {
    if (current_width_ == width && current_height_ == height && 
        ((is_hdr && p010_texture_) || (!is_hdr && nv12_texture_))) {
        return Result<void>();
    }

    current_width_ = width;
    current_height_ = height;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = is_hdr ? DXGI_FORMAT_P010 : DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = d3d_device_->CreateTexture2D(&desc, nullptr, &tex);
    if (FAILED(hr)) {
        return Error(ErrorCode::SystemError, "Failed to create D3D11Texture2D for video processor output");
    }

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc = {};
    content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    content_desc.InputFrameRate = {60, 1};
    content_desc.InputWidth = width;
    content_desc.InputHeight = height;
    content_desc.OutputFrameRate = {60, 1};
    content_desc.OutputWidth = width;
    content_desc.OutputHeight = height;
    content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    hr = video_device_->CreateVideoProcessorEnumerator(&content_desc, &video_enum_);
    if (FAILED(hr)) {
        return Error(ErrorCode::SystemError, "Failed to create ID3D11VideoProcessorEnumerator");
    }

    hr = video_device_->CreateVideoProcessor(video_enum_.Get(), 0, &video_processor_);
    if (FAILED(hr)) {
        return Error(ErrorCode::SystemError, "Failed to create ID3D11VideoProcessor");
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ov_desc = {};
    ov_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    ov_desc.Texture2D.MipSlice = 0;

    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> ov;
    hr = video_device_->CreateVideoProcessorOutputView(tex.Get(), video_enum_.Get(), &ov_desc, &ov);
    if (FAILED(hr)) {
        return Error(ErrorCode::SystemError, "Failed to create ID3D11VideoProcessorOutputView");
    }

    if (is_hdr) {
        p010_texture_ = tex;
        p010_output_view_ = ov;
    } else {
        nv12_texture_ = tex;
        nv12_output_view_ = ov;
    }

    // Set default color space
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE color_space = {};
    color_space.Usage = 0; // Playback
    color_space.RGB_Range = 0; // Full range
    color_space.YCbCr_Matrix = is_hdr ? 1 : 0; // 1 = BT.2020, 0 = BT.709
    color_space.YCbCr_xvYCC = 0;
    
    video_context_->VideoProcessorSetOutputColorSpace(video_processor_.Get(), &color_space);

    return Result<void>();
}

Result<Microsoft::WRL::ComPtr<ID3D11Texture2D>> D3D11ColorConverter::convert(Microsoft::WRL::ComPtr<ID3D11Texture2D> src_texture, bool request_hdr) {
    if (!video_device_ || !video_context_) {
        return Error(ErrorCode::InvalidParameter, "D3D11ColorConverter not initialized");
    }

    D3D11_TEXTURE2D_DESC src_desc;
    src_texture->GetDesc(&src_desc);

    UD_TRY(ensure_output_texture(src_desc.Width, src_desc.Height, request_hdr));

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC iv_desc = {};
    iv_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    iv_desc.Texture2D.MipSlice = 0;
    iv_desc.Texture2D.ArraySlice = 0;

    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> input_view;
    HRESULT hr = video_device_->CreateVideoProcessorInputView(src_texture.Get(), video_enum_.Get(), &iv_desc, &input_view);
    if (FAILED(hr)) {
        return Error(ErrorCode::SystemError, "Failed to create ID3D11VideoProcessorInputView");
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream_data = {};
    stream_data.Enable = TRUE;
    stream_data.OutputIndex = 0;
    stream_data.InputFrameOrField = 0;
    stream_data.PastFrames = 0;
    stream_data.FutureFrames = 0;
    stream_data.pInputSurface = input_view.Get();

    D3D11_VIDEO_PROCESSOR_COLOR_SPACE color_space = {};
    color_space.Usage = 0; // Playback
    color_space.RGB_Range = 0; // Full range
    color_space.YCbCr_Matrix = request_hdr ? 1 : 0;
    video_context_->VideoProcessorSetStreamColorSpace(video_processor_.Get(), 0, &color_space);

    auto out_view = request_hdr ? p010_output_view_.Get() : nv12_output_view_.Get();

    hr = video_context_->VideoProcessorBlt(video_processor_.Get(), out_view, 0, 1, &stream_data);
    if (FAILED(hr)) {
        return Error(ErrorCode::SystemError, "VideoProcessorBlt failed during color conversion");
    }

    return request_hdr ? p010_texture_ : nv12_texture_;
}

} // namespace ud
