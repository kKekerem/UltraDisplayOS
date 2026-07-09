#pragma once

#include "shared/transport/transport.hpp"
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>


#include <windows.h>
#include <winsock2.h>
#include <mswsock.h>

namespace ud {

class ServerTransport : public ITransport {
public:
    ServerTransport();
    ~ServerTransport() override;

    Result<void> init(uint16_t bind_port);

    // ITransport implementation
    Result<void> send(std::span<const uint8_t> payload, uint8_t stream_id) override;
    Result<std::span<const uint8_t>> receive(uint32_t timeout_us) override;
    void set_bandwidth_limit(uint64_t bps) override;
    void enable_multipath(bool enable) override;
    void set_dscp(uint8_t dscp_value) override;
    TransportStats stats() const override;

private:
    SOCKET socket_{INVALID_SOCKET};
    HANDLE iocp_{nullptr};
    
    std::thread worker_thread_;
    std::atomic<bool> running_{false};

    TransportStats current_stats_{};
    
    // IOCP specific context
    struct IoContext {
        WSAOVERLAPPED overlapped;
        WSABUF wsa_buf;
        uint8_t buffer[1500]; // MTU
        bool is_read;
    };

    void worker_loop();
    void post_receive();
};

} // namespace ud
