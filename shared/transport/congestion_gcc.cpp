#include "congestion_gcc.hpp"
#include <algorithm>
#include <cmath>

namespace ud {

GoogleCongestionControl::GoogleCongestionControl() {
    history_.reserve(1000);
}

void GoogleCongestionControl::on_packet_sent(uint16_t sequence_number, size_t size, Timestamp send_time) {
    if (history_.size() >= 1000) {
        history_.erase(history_.begin());
    }
    history_.push_back({sequence_number, size, send_time});
}

void GoogleCongestionControl::on_packet_acked(uint16_t sequence_number, Timestamp receive_time) {
    auto it = std::find_if(history_.begin(), history_.end(),
        [sequence_number](const PacketInfo& info) {
            return info.sequence_number == sequence_number;
        });

    if (it != history_.end()) {
        if (last_send_time_ != 0 && last_arrival_time_ != 0) {
            double send_delta = static_cast<double>(it->send_time) - static_cast<double>(last_send_time_);
            double recv_delta = static_cast<double>(receive_time) - static_cast<double>(last_arrival_time_);
            
            if (send_delta > 0) {
                double delay_gradient = recv_delta - send_delta;
                update_trendline(delay_gradient);
            }
        }
        
        last_send_time_ = it->send_time;
        last_arrival_time_ = receive_time;
        
        packets_since_last_loss_update_++;
        history_.erase(it);
    }
    
    update_bitrate_estimate();
}

void GoogleCongestionControl::on_packet_lost(uint16_t sequence_number) {
    (void)sequence_number;
    lost_packets_++;
    packets_since_last_loss_update_++;
    update_bitrate_estimate();
}

void GoogleCongestionControl::update_trendline(double delay_gradient) {
    // Simple EWMA trendline approximation
    const double alpha = 0.9;
    delay_gradient_ = alpha * delay_gradient_ + (1.0 - alpha) * delay_gradient;
    trendline_slope_ = delay_gradient_; // Simplified
}

void GoogleCongestionControl::update_bitrate_estimate() {
    // Delay-based control
    if (trendline_slope_ > 5.0) { // Overuse
        target_bitrate_bps_ = static_cast<uint64_t>(static_cast<double>(target_bitrate_bps_) * 0.85);
    } else if (trendline_slope_ < -5.0) { // Underuse
        target_bitrate_bps_ = static_cast<uint64_t>(static_cast<double>(target_bitrate_bps_) * 1.05);
    }

    // Loss-based control
    if (packets_since_last_loss_update_ >= 100) {
        loss_ratio_ = static_cast<float>(lost_packets_) / static_cast<float>(packets_since_last_loss_update_);
        if (loss_ratio_ > 0.1f) {
            target_bitrate_bps_ = static_cast<uint64_t>(static_cast<double>(target_bitrate_bps_) * (1.0 - 0.5 * loss_ratio_));
        } else if (loss_ratio_ < 0.02f) {
            target_bitrate_bps_ = static_cast<uint64_t>(static_cast<double>(target_bitrate_bps_) * 1.05);
        }
        lost_packets_ = 0;
        packets_since_last_loss_update_ = 0;
    }

    target_bitrate_bps_ = std::clamp(target_bitrate_bps_, min_bitrate_bps_, max_bitrate_bps_);
}

uint64_t GoogleCongestionControl::get_target_bitrate_bps() const {
    return target_bitrate_bps_;
}

void GoogleCongestionControl::set_min_bitrate(uint64_t bps) {
    min_bitrate_bps_ = bps;
    target_bitrate_bps_ = std::max(target_bitrate_bps_, min_bitrate_bps_);
}

void GoogleCongestionControl::set_max_bitrate(uint64_t bps) {
    max_bitrate_bps_ = bps;
    target_bitrate_bps_ = std::min(target_bitrate_bps_, max_bitrate_bps_);
}

} // namespace ud
