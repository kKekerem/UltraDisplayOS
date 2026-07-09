#include "encoder_factory.hpp"
#include "nvenc_encoder.hpp"
#include "amf_encoder.hpp"
#include "qsv_encoder.hpp"
#include "shared/util/log.hpp"


#include <windows.h>

namespace ud {

Result<std::unique_ptr<IEncoder>> EncoderFactory::create(VideoCodec codec) {
    if (is_nvenc_available()) {
        return std::unique_ptr<IEncoder>(new NvencEncoder());
    }
    
    if (is_amf_available()) {
        return std::unique_ptr<IEncoder>(new AmfEncoder());
    }
    
    if (is_qsv_available()) {
        return std::unique_ptr<IEncoder>(new QsvEncoder());
    }

    return Error(ErrorCode::NotImplemented, "No supported hardware encoder found");
}

bool EncoderFactory::is_nvenc_available() {
    HMODULE lib = LoadLibraryA("nvEncodeAPI64.dll");
    if (lib) {
        FreeLibrary(lib);
        return true;
    }
    return false;
}

bool EncoderFactory::is_amf_available() {
    HMODULE lib = LoadLibraryA("amfrt64.dll");
    if (lib) {
        FreeLibrary(lib);
        return true;
    }
    return false;
}

bool EncoderFactory::is_qsv_available() {
    HMODULE lib = LoadLibraryA("libmfxhw64.dll");
    if (!lib) {
        lib = LoadLibraryA("libvpl.dll"); // VPL fallback
    }
    if (lib) {
        FreeLibrary(lib);
        return true;
    }
    return false;
}

} // namespace ud
