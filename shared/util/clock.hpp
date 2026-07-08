#pragma once

#include <chrono>
#include <cstdint>

namespace ud {

using Timestamp = uint64_t; // microseconds
using Duration = int64_t;   // microseconds

inline Timestamp now_us() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<Timestamp>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

inline Duration elapsed_us(Timestamp start) {
    return static_cast<Duration>(now_us() - start);
}

// NTP-style clock sync offset tracking
class ClockSync {
public:
    ClockSync() = default;

    // Call this when receiving a Pong that replies to a Ping.
    // t0: Time ping was sent (local)
    // t1: Time ping received by peer (remote)
    // t2: Time pong sent by peer (remote)
    // t3: Time pong received (local)
    void update(Timestamp t0, Timestamp t1, Timestamp t2, Timestamp t3) {
        Duration rtt = (t3 - t0) - (t2 - t1);
        Duration offset = ((t1 - t0) + (t2 - t3)) / 2;
        
        // Simple smoothing
        if (initialized_) {
            smoothed_rtt_ = (smoothed_rtt_ * 7 + rtt) / 8;
            offset_ = (offset_ * 7 + offset) / 8;
        } else {
            smoothed_rtt_ = rtt;
            offset_ = offset;
            initialized_ = true;
        }
    }

    Timestamp local_to_remote(Timestamp local) const {
        return local + offset_;
    }

    Timestamp remote_to_local(Timestamp remote) const {
        return remote - offset_;
    }

    Duration rtt() const { return smoothed_rtt_; }
    Duration offset() const { return offset_; }

private:
    bool initialized_ = false;
    Duration smoothed_rtt_{0};
    Duration offset_{0};
};

} // namespace ud
