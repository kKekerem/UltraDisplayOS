#pragma once

#include "shared/util/clock.hpp"
#include <cstdint>
#include <vector>

namespace ud {

class GoogleCongestionControl {
public:
    GoogleCongestionControl();
    ~GoogleCongestionControl() = default;

    void on_packet_sent(uint16_t sequence_number, size_t size, Timestamp send_time);
    void on_packet_acked(uint16_t sequence_number, Timestamp receive_time);
    void on_packet_lost(uint16_t sequence_number);

    uint64_t get_target_bitrate_bps() const;
    void set_min_bitrate(uint64_t bps);
    void set_max_bitrate(uint64_t bps);

private:
    uint64_t target_bitrate_bps_{1000000};
    uint64_t min_bitrate_bps_{100000};
    uint64_t max_bitrate_bps_{100000000};

    // Trendline filter state
    struct PacketInfo {
        uint16_t sequence_number;
        size_t size;
        Timestamp send_time;
    };

    std::vector<PacketInfo> history_;
    double trendline_slope_{0.0};
    
    // Arrival time filter
    Timestamp last_arrival_time_{0};
    Timestamp last_send_time_{0};
    double delay_gradient_{0.0};

    // Loss-based control
    float loss_ratio_{0.0};
    size_t packets_since_last_loss_update_{0};
    size_t lost_packets_{0};

    void update_trendline(double delay_gradient);
    void update_bitrate_estimate();
};

} // namespace ud
