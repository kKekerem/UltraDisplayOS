#include "encoder_factory.hpp"
#include "encoder_sdk_config.hpp"
#include "nvenc_encoder.hpp"
#include "amf_encoder.hpp"
#include "qsv_encoder.hpp"
#include "shared/util/log.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ud {

Result<std::unique_ptr<IEncoder>> EncoderFactory::create(VideoCodec codec) {
    static_cast<void>(codec);

#if UD_HAS_NVENC_SDK
    if (is_nvenc_available()) {
        auto encoder = std::make_unique<NvencEncoder>();
        return std::move(encoder);
    }
#endif

#if UD_HAS_AMF_SDK
    if (is_amf_available()) {
        auto encoder = std::make_unique<AmfEncoder>();
        return std::move(encoder);
    }
#endif

#if UD_HAS_QSV_SDK
    if (is_qsv_available()) {
        auto encoder = std::make_unique<QsvEncoder>();
        return std::move(encoder);
    }
#endif

    return Error(ErrorCode::NotImplemented, "No supported hardware encoder found");
}

bool EncoderFactory::is_nvenc_available() {
#if UD_HAS_NVENC_SDK
    HMODULE lib = LoadLibraryA("nvEncodeAPI64.dll");
    if (lib) {
        FreeLibrary(lib);
        return true;
    }
#endif
    return false;
}

bool EncoderFactory::is_amf_available() {
#if UD_HAS_AMF_SDK
    HMODULE lib = LoadLibraryA("amfrt64.dll");
    if (lib) {
        FreeLibrary(lib);
        return true;
    }
#endif
    return false;
}

bool EncoderFactory::is_qsv_available() {
#if UD_HAS_QSV_SDK
    HMODULE lib = LoadLibraryA("libmfxhw64.dll");
    if (!lib) {
        lib = LoadLibraryA("libvpl.dll"); // VPL fallback
    }
    if (lib) {
        FreeLibrary(lib);
        return true;
    }
#endif
    return false;
}

} // namespace ud
