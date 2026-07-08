#include "pacer.hpp"
#include <thread>
#include <algorithm>

namespace ud {

PacketPacer::PacketPacer() : last_update_us_(now_us()) {
}

void PacketPacer::set_rate(uint64_t bps) {
    rate_bps_ = bps;
    // Max tokens equals enough to send for 10ms burst
    max_tokens_ = (bps / 8) / 100;
    if (tokens_ > max_tokens_) {
        tokens_ = max_tokens_;
    }
}

void PacketPacer::refill_tokens() {
    Timestamp now = now_us();
    Duration elapsed = now - last_update_us_;
    if (elapsed > 0 && rate_bps_ > 0) {
        int64_t new_tokens = (static_cast<int64_t>(rate_bps_) * elapsed) / (8 * 1000000);
        tokens_ = std::min(max_tokens_, tokens_ + new_tokens);
        last_update_us_ = now;
    }
}

bool PacketPacer::can_send(size_t packet_bytes) {
    refill_tokens();
    return tokens_ >= static_cast<int64_t>(packet_bytes);
}

void PacketPacer::record_send(size_t packet_bytes) {
    tokens_ -= static_cast<int64_t>(packet_bytes);
}

void PacketPacer::wait_until_can_send(size_t packet_bytes) {
    while (!can_send(packet_bytes)) {
        int64_t missing_tokens = static_cast<int64_t>(packet_bytes) - tokens_;
        if (rate_bps_ > 0 && missing_tokens > 0) {
            int64_t sleep_us = (missing_tokens * 8 * 1000000) / rate_bps_;
            std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
        } else {
            std::this_thread::yield();
        }
    }
}

} // namespace ud
