#include "vaapi_decoder.hpp"
#include <fcntl.h>
#include <va/va_drmcommon.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <drm_fourcc.h>

namespace ud {

VaapiDecoder::VaapiDecoder() = default;

VaapiDecoder::~VaapiDecoder() {
    if (va_ctx_ != VA_INVALID_ID) {
        vaDestroyContext(va_dpy_, va_ctx_);
    }
    if (va_config_ != VA_INVALID_ID) {
        vaDestroyConfig(va_dpy_, va_config_);
    }
    if (!surfaces_.empty()) {
        vaDestroySurfaces(va_dpy_, surfaces_.data(), surfaces_.size());
    }
    if (va_dpy_) {
        vaTerminate(va_dpy_);
    }
}

Result<void> VaapiDecoder::init(int drm_fd, VideoCodec codec, uint32_t width, uint32_t height) {
    va_dpy_ = vaGetDisplayDRM(drm_fd);
    if (!va_dpy_) {
        return Error(ErrorCode::SystemError, "vaGetDisplayDRM failed");
    }

    int major_ver, minor_ver;
    if (vaInitialize(va_dpy_, &major_ver, &minor_ver) != VA_STATUS_SUCCESS) {
        return Error(ErrorCode::SystemError, "vaInitialize failed");
    }

    VAProfile profile = VAProfileH264Main;
    if (codec == VideoCodec::HEVC) {
        profile = VAProfileHEVCMain;
    } else if (codec == VideoCodec::AV1) {
        profile = VAProfileAV1Profile0;
    }

    VAConfigAttrib attrib;
    attrib.type = VAConfigAttribRTFormat;
    attrib.value = VA_RT_FORMAT_YUV420;

    if (vaCreateConfig(va_dpy_, profile, VAEntrypointVLD, &attrib, 1, &va_config_) != VA_STATUS_SUCCESS) {
        return Error(ErrorCode::SystemError, "vaCreateConfig failed");
    }

    UD_TRY(create_surfaces(width, height));

    if (vaCreateContext(va_dpy_, va_config_, width, height, VA_PROGRESSIVE, 
                        surfaces_.data(), surfaces_.size(), &va_ctx_) != VA_STATUS_SUCCESS) {
        return Error(ErrorCode::SystemError, "vaCreateContext failed");
    }

    return Result<void>();
}

Result<void> VaapiDecoder::create_surfaces(uint32_t width, uint32_t height) {
    surfaces_.resize(8); // 8 surfaces for decode pool
    VASurfaceAttrib attribs[1] = {};
    attribs[0].type = VASurfaceAttribPixelFormat;
    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_FOURCC_NV12;

    if (vaCreateSurfaces(va_dpy_, VA_RT_FORMAT_YUV420, width, height,
                         surfaces_.data(), surfaces_.size(), attribs, 1) != VA_STATUS_SUCCESS) {
        return Error(ErrorCode::SystemError, "vaCreateSurfaces failed");
    }
    return Result<void>();
}

Result<void> VaapiDecoder::submit_packet(std::span<const uint8_t> packet_data, uint32_t pts_us) {
    VASurfaceID target_surface = surfaces_[0]; // Simplification for demo
    
    if (vaBeginPicture(va_dpy_, va_ctx_, target_surface) != VA_STATUS_SUCCESS) {
        return Error(ErrorCode::SystemError, "vaBeginPicture failed");
    }

    VABufferID slice_data_buf;
    if (vaCreateBuffer(va_dpy_, va_ctx_, VASliceDataBufferType, packet_data.size(), 1, 
                       const_cast<void*>(reinterpret_cast<const void*>(packet_data.data())), &slice_data_buf) != VA_STATUS_SUCCESS) {
        vaEndPicture(va_dpy_, va_ctx_);
        return Error(ErrorCode::SystemError, "vaCreateBuffer failed");
    }

    if (vaRenderPicture(va_dpy_, va_ctx_, &slice_data_buf, 1) != VA_STATUS_SUCCESS) {
        vaEndPicture(va_dpy_, va_ctx_);
        return Error(ErrorCode::SystemError, "vaRenderPicture failed");
    }

    if (vaEndPicture(va_dpy_, va_ctx_) != VA_STATUS_SUCCESS) {
        return Error(ErrorCode::SystemError, "vaEndPicture failed");
    }

    return Result<void>();
}

Result<DecodedFrame> VaapiDecoder::get_frame(uint32_t timeout_us) {
    VASurfaceID ready_surface = surfaces_[0]; // Simplification for sync retrieval
    
    if (vaSyncSurface(va_dpy_, ready_surface) != VA_STATUS_SUCCESS) {
        return Error(ErrorCode::SystemError, "vaSyncSurface failed");
    }

    VADRMPRIMESurfaceDescriptor drm_desc;
    if (vaExportSurfaceHandle(va_dpy_, ready_surface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2, 
                              VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS, 
                              &drm_desc) != VA_STATUS_SUCCESS) {
        return Error(ErrorCode::SystemError, "vaExportSurfaceHandle failed");
    }

    DecodedFrame frame;
    frame.dma_buf_fd = drm_desc.objects[0].fd;
    frame.width = drm_desc.width;
    frame.height = drm_desc.height;
    frame.drm_format = drm_desc.layers[0].drm_format;
    frame.stride = drm_desc.layers[0].pitch[0];
    frame.pts_us = 0; 

    return frame;
}

} // namespace ud
