#include "nvenc_encoder.hpp"
#include "shared/util/log.hpp"
#include <nvEncodeAPI.h>
#include <stdexcept>

typedef NVENCSTATUS (NVENCAPI *PNVENCODEAPICREATEINSTANCE)(NV_ENCODE_API_FUNCTION_LIST *functionList);

namespace ud {

class NvencImpl {
public:
    NvencImpl() {
        nvenc_lib_ = LoadLibraryA("nvEncodeAPI64.dll");
        if (!nvenc_lib_) throw std::runtime_error("nvEncodeAPI64.dll not found");
        
        auto NvEncodeAPICreateInstance = (PNVENCODEAPICREATEINSTANCE)GetProcAddress(nvenc_lib_, "NvEncodeAPICreateInstance");
        if (!NvEncodeAPICreateInstance) throw std::runtime_error("NvEncodeAPICreateInstance not found");
        
        nvenc_funcs_.version = NV_ENCODE_API_FUNCTION_LIST_VER;
        if (NvEncodeAPICreateInstance(&nvenc_funcs_) != NV_ENC_SUCCESS) {
            throw std::runtime_error("Failed to create NVENC instance");
        }
    }
    
    ~NvencImpl() {
        if (encoder_) {
            nvenc_funcs_.nvEncDestroyEncoder(encoder_);
        }
        if (nvenc_lib_) {
            FreeLibrary(nvenc_lib_);
        }
    }
    
    Result<void> init(Microsoft::WRL::ComPtr<ID3D11Device> d3d_device, const EncoderConfig& config) {
        d3d_device_ = d3d_device;
        
        NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS session_params = {};
        session_params.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
        session_params.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
        session_params.device = d3d_device.Get();
        session_params.apiVersion = NVENCAPI_VERSION;
        
        if (nvenc_funcs_.nvEncOpenEncodeSessionEx(&session_params, &encoder_) != NV_ENC_SUCCESS) {
            return Error(ErrorCode::SystemError, "nvEncOpenEncodeSessionEx failed");
        }
        
        return reconfigure(config);
    }
    
    Result<void> reconfigure(const EncoderConfig& config) {
        NV_ENC_INITIALIZE_PARAMS init_params = {};
        init_params.version = NV_ENC_INITIALIZE_PARAMS_VER;
        init_params.encodeGUID = get_codec_guid(config.codec);
        init_params.presetGUID = NV_ENC_PRESET_P1_GUID;
        init_params.encodeWidth = 1920; 
        init_params.encodeHeight = 1080;
        init_params.darWidth = 1920;
        init_params.darHeight = 1080;
        init_params.frameRateNum = config.framerate;
        init_params.frameRateDen = 1;
        init_params.enablePTD = 1;
        init_params.reportSliceOffsets = 0;
        
        NV_ENC_CONFIG enc_config = {};
        enc_config.version = NV_ENC_CONFIG_VER;
        init_params.encodeConfig = &enc_config;
        
        NV_ENC_PRESET_CONFIG preset_config = {};
        preset_config.version = NV_ENC_PRESET_CONFIG_VER;
        preset_config.presetCfg.version = NV_ENC_CONFIG_VER;
        
        nvenc_funcs_.nvEncGetEncodePresetConfig(encoder_, init_params.encodeGUID, init_params.presetGUID, &preset_config);
        memcpy(&enc_config, &preset_config.presetCfg, sizeof(NV_ENC_CONFIG));
        
        enc_config.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
        enc_config.rcParams.averageBitRate = config.bitrate_bps;
        enc_config.gopLength = config.force_idr ? 1 : NVENC_INFINITE_GOPLENGTH;
        enc_config.frameIntervalP = 1;
        
        if (config.codec == VideoCodec::H264) {
            enc_config.encodeCodecConfig.h264Config.idrPeriod = enc_config.gopLength;
            enc_config.encodeCodecConfig.h264Config.repeatSPSPPS = 1;
        } else if (config.codec == VideoCodec::HEVC) {
            enc_config.encodeCodecConfig.hevcConfig.idrPeriod = enc_config.gopLength;
            enc_config.encodeCodecConfig.hevcConfig.repeatSPSPPS = 1;
        }
        
        if (nvenc_funcs_.nvEncInitializeEncoder(encoder_, &init_params) != NV_ENC_SUCCESS) {
            return Error(ErrorCode::SystemError, "nvEncInitializeEncoder failed");
        }

        if (!bitstream_buffer_) {
            NV_ENC_CREATE_BITSTREAM_BUFFER bs_buf = {};
            bs_buf.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
            if (nvenc_funcs_.nvEncCreateBitstreamBuffer(encoder_, &bs_buf) == NV_ENC_SUCCESS) {
                bitstream_buffer_ = bs_buf.bitstreamBuffer;
            }
        }
        
        return Result<void>();
    }
    
    Result<std::vector<EncodedPacket>> encode(Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture) {
        if (!registered_resource_) {
            NV_ENC_REGISTER_RESOURCE reg_res = {};
            reg_res.version = NV_ENC_REGISTER_RESOURCE_VER;
            reg_res.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
            reg_res.resourceToRegister = nv12_texture.Get();
            reg_res.width = 1920;
            reg_res.height = 1080;
            reg_res.pitch = 0;
            reg_res.bufferFormat = NV_ENC_BUFFER_FORMAT_NV12;
            
            if (nvenc_funcs_.nvEncRegisterResource(encoder_, &reg_res) != NV_ENC_SUCCESS) {
                return Error(ErrorCode::SystemError, "nvEncRegisterResource failed");
            }
            registered_resource_ = reg_res.registeredResource;
        }
        
        NV_ENC_MAP_INPUT_RESOURCE map_res = {};
        map_res.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
        map_res.registeredResource = registered_resource_;
        
        if (nvenc_funcs_.nvEncMapInputResource(encoder_, &map_res) != NV_ENC_SUCCESS) {
            return Error(ErrorCode::SystemError, "nvEncMapInputResource failed");
        }
        
        NV_ENC_PIC_PARAMS pic_params = {};
        pic_params.version = NV_ENC_PIC_PARAMS_VER;
        pic_params.inputWidth = 1920;
        pic_params.inputHeight = 1080;
        pic_params.inputPitch = pic_params.inputWidth;
        pic_params.inputBuffer = map_res.mappedResource;
        pic_params.outputBitstream = bitstream_buffer_;
        pic_params.bufferFmt = map_res.mappedBufferFmt;
        pic_params.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
        
        if (nvenc_funcs_.nvEncEncodePicture(encoder_, &pic_params) != NV_ENC_SUCCESS) {
            nvenc_funcs_.nvEncUnmapInputResource(encoder_, map_res.mappedResource);
            return Error(ErrorCode::SystemError, "nvEncEncodePicture failed");
        }
        
        nvenc_funcs_.nvEncUnmapInputResource(encoder_, map_res.mappedResource);
        
        NV_ENC_LOCK_BITSTREAM lock_bs = {};
        lock_bs.version = NV_ENC_LOCK_BITSTREAM_VER;
        lock_bs.outputBitstream = bitstream_buffer_;
        
        std::vector<EncodedPacket> packets;
        if (nvenc_funcs_.nvEncLockBitstream(encoder_, &lock_bs) == NV_ENC_SUCCESS) {
            EncodedPacket packet;
            packet.data.assign(static_cast<uint8_t*>(lock_bs.bitstreamBufferPtr),
                               static_cast<uint8_t*>(lock_bs.bitstreamBufferPtr) + lock_bs.bitstreamSizeInBytes);
            packet.is_keyframe = (lock_bs.pictureType == NV_ENC_PIC_TYPE_IDR);
            packet.frame_number = static_cast<uint16_t>(lock_bs.frameIdx);
            packets.push_back(std::move(packet));
            
            nvenc_funcs_.nvEncUnlockBitstream(encoder_, bitstream_buffer_);
        }
        
        return packets;
    }
    
private:
    GUID get_codec_guid(VideoCodec codec) {
        if (codec == VideoCodec::HEVC) return NV_ENC_CODEC_HEVC_GUID;
        if (codec == VideoCodec::AV1) return NV_ENC_CODEC_AV1_GUID;
        return NV_ENC_CODEC_H264_GUID;
    }

    HMODULE nvenc_lib_{nullptr};
    NV_ENCODE_API_FUNCTION_LIST nvenc_funcs_{};
    void* encoder_{nullptr};
    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
    void* registered_resource_{nullptr};
    void* bitstream_buffer_{nullptr};
};

NvencEncoder::NvencEncoder() : impl_(std::make_unique<NvencImpl>()) {}
NvencEncoder::~NvencEncoder() = default;

Result<void> NvencEncoder::init(Microsoft::WRL::ComPtr<ID3D11Device> d3d_device, const EncoderConfig& config) {
    return impl_->init(d3d_device, config);
}

Result<void> NvencEncoder::reconfigure(const EncoderConfig& config) {
    return impl_->reconfigure(config);
}

Result<std::vector<EncodedPacket>> NvencEncoder::encode(Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture) {
    return impl_->encode(nv12_texture);
}

} // namespace ud
