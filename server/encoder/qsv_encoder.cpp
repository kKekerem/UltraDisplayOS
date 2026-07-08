#include "qsv_encoder.hpp"
#include <vpl/mfx.h>
#include <vpl/mfxvideo.h>
#include <stdexcept>

namespace ud {

class QsvImpl {
public:
    QsvImpl() {
        mfxStatus sts = MFXInit(MFX_IMPL_HARDWARE, &version_, &session_);
        if (sts != MFX_ERR_NONE) {
            throw std::runtime_error("MFXInit failed");
        }
    }
    
    ~QsvImpl() {
        if (session_) {
            if (encoder_initialized_) MFXVideoENCODE_Close(session_);
            MFXClose(session_);
        }
        if (bs_.Data) {
            delete[] bs_.Data;
        }
    }
    
    Result<void> init(Microsoft::WRL::ComPtr<ID3D11Device> d3d_device, const EncoderConfig& config) {
        MFXVideoCORE_SetHandle(session_, MFX_HANDLE_D3D11_DEVICE, d3d_device.Get());
        return reconfigure(config);
    }
    
    Result<void> reconfigure(const EncoderConfig& config) {
        mfxVideoParam params = {};
        params.mfx.CodecId = (config.codec == VideoCodec::HEVC) ? MFX_CODEC_HEVC : MFX_CODEC_AVC;
        params.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
        params.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
        params.mfx.TargetKbps = config.bitrate_bps / 1000;
        params.mfx.FrameInfo.FrameRateExtN = config.framerate;
        params.mfx.FrameInfo.FrameRateExtD = 1;
        params.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        params.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        params.mfx.FrameInfo.CropW = 1920;
        params.mfx.FrameInfo.CropH = 1080;
        params.mfx.FrameInfo.Width = 1920;
        params.mfx.FrameInfo.Height = 1080;
        params.mfx.GopRefDist = 1; // No B-frames
        params.mfx.GopPicSize = config.force_idr ? 1 : 0xFFFF; // Infinite GOP
        
        params.AsyncDepth = 1;
        
        if (!encoder_initialized_) {
            mfxStatus sts = MFXVideoENCODE_Init(session_, &params);
            if (sts != MFX_ERR_NONE) {
                return Error(ErrorCode::SystemError, "MFXVideoENCODE_Init failed");
            }
            encoder_initialized_ = true;
            
            mfxVideoParam actual_params;
            MFXVideoENCODE_GetVideoParam(session_, &actual_params);
            
            bs_.MaxLength = actual_params.mfx.BufferSizeInKB * 1000;
            bs_.Data = new mfxU8[bs_.MaxLength];
        } else {
            MFXVideoENCODE_Reset(session_, &params);
        }
        
        return Result<void>();
    }
    
    Result<std::vector<EncodedPacket>> encode(Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture) {
        mfxFrameSurface1 surface = {};
        surface.Info.FourCC = MFX_FOURCC_NV12;
        surface.Info.Width = 1920;
        surface.Info.Height = 1080;
        surface.Info.CropW = 1920;
        surface.Info.CropH = 1080;
        surface.Data.MemId = nv12_texture.Get();
        
        mfxSyncPoint syncp;
        mfxStatus sts = MFXVideoENCODE_EncodeFrameAsync(session_, nullptr, &surface, &bs_, &syncp);
        
        std::vector<EncodedPacket> packets;
        if (sts == MFX_ERR_NONE && syncp) {
            MFXVideoCORE_SyncOperation(session_, syncp, 1000);
            
            EncodedPacket packet;
            packet.data.assign(bs_.Data + bs_.DataOffset, bs_.Data + bs_.DataOffset + bs_.DataLength);
            packet.is_keyframe = (bs_.FrameType & MFX_FRAMETYPE_IDR);
            packet.frame_number = static_cast<uint16_t>(frame_count_++);
            packets.push_back(std::move(packet));
            
            bs_.DataLength = 0;
            bs_.DataOffset = 0;
        }
        
        return packets;
    }
    
private:
    mfxSession session_{nullptr};
    mfxVersion version_{1, 0};
    bool encoder_initialized_{false};
    mfxBitstream bs_ = {};
    uint32_t frame_count_{0};
};

QsvEncoder::QsvEncoder() : impl_(std::make_unique<QsvImpl>()) {}
QsvEncoder::~QsvEncoder() = default;

Result<void> QsvEncoder::init(Microsoft::WRL::ComPtr<ID3D11Device> d3d_device, const EncoderConfig& config) {
    return impl_->init(d3d_device, config);
}

Result<void> QsvEncoder::reconfigure(const EncoderConfig& config) {
    return impl_->reconfigure(config);
}

Result<std::vector<EncodedPacket>> QsvEncoder::encode(Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture) {
    return impl_->encode(nv12_texture);
}

} // namespace ud
