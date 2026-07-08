#pragma once

#include "shared/util/result.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace ud {

struct TransportStats {
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t rtt_us;
    float packet_loss_percent;
    uint64_t bandwidth_est_bps;
};

class ITransport {
public:
    virtual ~ITransport() = default;

    // Send a raw packet payload. Transport handles framing/headers.
    virtual Result<void> send(std::span<const uint8_t> payload, uint8_t stream_id) = 0;
    
    // Receive next available payload. Returns span over internal buffer.
    virtual Result<std::span<const uint8_t>> receive(uint32_t timeout_us) = 0;

    // Congestion control limits
    virtual void set_bandwidth_limit(uint64_t bps) = 0;

    // Multipath features
    virtual void enable_multipath(bool enable) = 0;

    // DSCP / ECN config
    virtual void set_dscp(uint8_t dscp_value) = 0;

    virtual TransportStats stats() const = 0;
};

} // namespace ud
