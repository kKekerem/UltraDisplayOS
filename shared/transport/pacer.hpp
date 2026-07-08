#pragma once

#include "shared/util/clock.hpp"
#include <cstdint>
#include <span>

namespace ud {

class PacketPacer {
public:
    PacketPacer();

    // Set pacing rate in bits per second
    void set_rate(uint64_t bps);
    
    // Returns true if packet can be sent now based on token bucket
    bool can_send(size_t packet_bytes);
    
    // Consume tokens for sent packet
    void record_send(size_t packet_bytes);
    
    // Wait until tokens are available
    void wait_until_can_send(size_t packet_bytes);

private:
    uint64_t rate_bps_{0};
    int64_t tokens_{0};
    int64_t max_tokens_{0};
    Timestamp last_update_us_{0};
    
    void refill_tokens();
};

} // namespace ud
