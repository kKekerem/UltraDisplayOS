#pragma once

#include "encoder.hpp"
#include <memory>

namespace ud {

class EncoderFactory {
public:
    // Probes available hardware and returns the best supported encoder for the given codec
    static Result<std::unique_ptr<IEncoder>> create(VideoCodec codec);

private:
    static bool is_nvenc_available();
    static bool is_amf_available();
    static bool is_qsv_available();
};

} // namespace ud
