#pragma once

#include "shared/util/result.hpp"
#include <span>
#include <string>

namespace ud {

class TlsContext {
public:
    TlsContext(bool is_server);
    ~TlsContext();

    // Initialize with self-signed certificate (generated on first boot)
    Result<void> load_credentials(const std::string& cert_path, const std::string& key_path);

    // TLS 1.3 handshake over existing TCP socket
    Result<void> handshake(int socket_fd);

    Result<void> send(std::span<const uint8_t> data);
    Result<std::span<const uint8_t>> receive(uint32_t timeout_us);

    // Verify peer's fingerprint against paired devices list
    bool verify_peer_fingerprint(const std::string& expected_fingerprint);

private:
    void* ssl_ctx_{nullptr};
    void* ssl_{nullptr};
    int fd_{-1};
    bool is_server_;
};

} // namespace ud
