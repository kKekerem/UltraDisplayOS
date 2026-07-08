#pragma once

#include "shared/util/result.hpp"
#include <string>

namespace ud {

class OtaVerifier {
public:
    OtaVerifier(const std::string& public_key_path);
    ~OtaVerifier() = default;

    // Stream verification for A/B partition OTA updates
    Result<void> begin_verify();
    Result<void> update_verify(const uint8_t* data, size_t length);
    Result<bool> finish_verify(const std::string& expected_signature_base64);

private:
    std::string public_key_path_;
    void* evp_md_ctx_{nullptr};
};

} // namespace ud
