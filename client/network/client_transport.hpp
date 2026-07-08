#pragma once

#include "shared/transport/transport.hpp"
#include <liburing.h>
#include <vector>
#include <thread>
#include <atomic>

namespace ud {

class ClientTransport : public ITransport {
public:
    ClientTransport();
    ~ClientTransport() override;

    Result<void> init(uint16_t bind_port);

    // ITransport implementation
    Result<void> send(std::span<const uint8_t> payload, uint8_t stream_id) override;
    Result<std::span<const uint8_t>> receive(uint32_t timeout_us) override;
    void set_bandwidth_limit(uint64_t bps) override;
    void enable_multipath(bool enable) override;
    void set_dscp(uint8_t dscp_value) override;
    TransportStats stats() const override;

private:
    int socket_fd_{-1};
    struct io_uring ring_;
    
    // Fixed buffers for zero-copy I/O with io_uring
    std::vector<std::vector<uint8_t>> recv_buffers_;
    struct iovec* iovecs_{nullptr};
    
    std::thread poll_thread_;
    std::atomic<bool> running_{false};

    TransportStats current_stats_{};

    void setup_uring();
    void submit_recv_multishot();
    void poll_loop();
};

} // namespace ud
