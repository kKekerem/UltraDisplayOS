#pragma once

#include "shared/transport/transport.hpp"
#include <ftxui/component/component.hpp>

namespace ud {

class ConnectionOverlay {
public:
    ConnectionOverlay();
    ~ConnectionOverlay() = default;

    // Update real-time stats for the overlay
    void update_stats(const TransportStats& stats, uint32_t current_fps, const std::string& codec_info);

    // Returns the FTXUI component
    ftxui::Component get_component();

private:
    TransportStats stats_{};
    uint32_t fps_{0};
    std::string codec_info_{"Unknown"};
    
    // Ring buffer history for the ASCII latency graph (60 seconds)
    std::vector<int> latency_history_;
    
    ftxui::Component renderer_;
    ftxui::Component build_ui();
};

} // namespace ud
