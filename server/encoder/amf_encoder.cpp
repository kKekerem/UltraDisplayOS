#include "amf_encoder.hpp"
#include <core/Factory.h>
#include <core/Context.h>
#include <components/VideoEncoderVCE.h>
#include <components/VideoEncoderHEVC.h>
#include <components/VideoEncoderAV1.h>
#include <stdexcept>

namespace ud {

class AmfImpl {
public:
    AmfImpl() {
        amf_lib_ = LoadLibraryA("amfrt64.dll");
        if (!amf_lib_) throw std::runtime_error("amfrt64.dll not found");
        
        auto init_func = (AMFInit_Fn)GetProcAddress(amf_lib_, AMF_INIT_FUNCTION_NAME);
        if (!init_func) throw std::runtime_error("AMFInit not found");
        
        if (init_func(AMF_FULL_VERSION, &factory_) != AMF_OK) {
            throw std::runtime_error("AMFInit failed");
        }
        
        factory_->CreateContext(&context_);
    }
    
    ~AmfImpl() {
        if (encoder_) {
            encoder_->Terminate();
        }
        if (context_) {
            context_->Terminate();
        }
        if (amf_lib_) {
            FreeLibrary(amf_lib_);
        }
    }
    
    Result<void> init(Microsoft::WRL::ComPtr<ID3D11Device> d3d_device, const EncoderConfig& config) {
        context_->InitDX11(d3d_device.Get());
        
        const wchar_t* codec_id = AMFVideoEncoderVCE_AVC;
        if (config.codec == VideoCodec::HEVC) {
            codec_id = AMFVideoEncoder_HEVC;
        } else if (config.codec == VideoCodec::AV1) {
            codec_id = AMFVideoEncoder_AV1;
        }
        
        if (factory_->CreateComponent(context_, codec_id, &encoder_) != AMF_OK) {
            return Error(ErrorCode::SystemError, "Failed to create AMF component");
        }
        
        return reconfigure(config);
    }
    
    Result<void> reconfigure(const EncoderConfig& config) {
        encoder_->SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_ULTRA_LOW_LATENCY);
        encoder_->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, config.bitrate_bps);
        encoder_->SetProperty(AMF_VIDEO_ENCODER_FRAMESIZE, ::AMFConstructSize(1920, 1080));
        encoder_->SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, ::AMFConstructRate(config.framerate, 1));
        encoder_->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
        encoder_->SetProperty(AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD, AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CBR);
        
        if (config.force_idr) {
            encoder_->SetProperty(AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE, AMF_VIDEO_ENCODER_PICTURE_TYPE_IDR);
        }
        
        if (!encoder_initialized_) {
            if (encoder_->Init(amf::AMF_SURFACE_NV12, 1920, 1080) != AMF_OK) {
                return Error(ErrorCode::SystemError, "AMF Encoder Init failed");
            }
            encoder_initialized_ = true;
        }
        
        return Result<void>();
    }
    
    Result<std::vector<EncodedPacket>> encode(Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture) {
        amf::AMFSurfacePtr surface;
        if (context_->CreateSurfaceFromDX11Native(nv12_texture.Get(), &surface, nullptr) != AMF_OK) {
            return Error(ErrorCode::SystemError, "AMF CreateSurfaceFromDX11Native failed");
        }
        
        if (encoder_->SubmitInput(surface) != AMF_OK) {
            return Error(ErrorCode::SystemError, "AMF SubmitInput failed");
        }
        
        std::vector<EncodedPacket> packets;
        amf::AMFDataPtr data;
        if (encoder_->QueryOutput(&data) == AMF_OK && data != nullptr) {
            amf::AMFBufferPtr buffer(data);
            if (buffer) {
                EncodedPacket packet;
                packet.data.assign(static_cast<uint8_t*>(buffer->GetNative()),
                                   static_cast<uint8_t*>(buffer->GetNative()) + buffer->GetSize());
                
                int64_t type = 0;
                data->GetProperty(AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE, &type);
                packet.is_keyframe = (type == AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_IDR);
                packet.frame_number = static_cast<uint16_t>(frame_count_++);
                packets.push_back(std::move(packet));
            }
        }
        
        return packets;
    }
    
private:
    HMODULE amf_lib_{nullptr};
    amf::AMFFactory* factory_{nullptr};
    amf::AMFContextPtr context_;
    amf::AMFComponentPtr encoder_;
    uint32_t frame_count_{0};
    bool encoder_initialized_{false};
};

AmfEncoder::AmfEncoder() : impl_(std::make_unique<AmfImpl>()) {}
AmfEncoder::~AmfEncoder() = default;

Result<void> AmfEncoder::init(Microsoft::WRL::ComPtr<ID3D11Device> d3d_device, const EncoderConfig& config) {
    return impl_->init(d3d_device, config);
}

Result<void> AmfEncoder::reconfigure(const EncoderConfig& config) {
    return impl_->reconfigure(config);
}

Result<std::vector<EncodedPacket>> AmfEncoder::encode(Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture) {
    return impl_->encode(nv12_texture);
}

} // namespace ud
