#pragma once

#include "shared/util/result.hpp"
#include "shared/protocol/codec_config.hpp"

#include <va/va.h>
#include <va/va_drm.h>
#include <vector>
#include <span>

namespace ud {

struct DecodedFrame {
    int dma_buf_fd;
    uint32_t width;
    uint32_t height;
    uint32_t drm_format; // e.g., DRM_FORMAT_NV12
    uint32_t stride;
    uint32_t pts_us;     // Presentation timestamp
};

class VaapiDecoder {
public:
    VaapiDecoder();
    ~VaapiDecoder();

    // Initialize VAAPI using the same DRM node as KMS to ensure zero-copy compatibility
    Result<void> init(int drm_fd, VideoCodec codec, uint32_t width, uint32_t height);

    // Feed encoded NAL units to the decoder.
    // Decoding is asynchronous.
    Result<void> submit_packet(std::span<const uint8_t> packet_data, uint32_t pts_us);

    // Retrieve a decoded frame ready for display.
    // Blocks until a frame is ready or timeout occurs.
    Result<DecodedFrame> get_frame(uint32_t timeout_us);

private:
    VADisplay va_dpy_{nullptr};
    VAContextID va_ctx_{VA_INVALID_ID};
    VAConfigID va_config_{VA_INVALID_ID};
    
    std::vector<VASurfaceID> surfaces_;
    
    Result<void> create_surfaces(uint32_t width, uint32_t height);
};

} // namespace ud
