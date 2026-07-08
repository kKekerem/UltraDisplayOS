#pragma once

#include "transport.hpp"
#include <vector>
#include <memory>

namespace ud {

struct PathStats {
    uint32_t rtt_us;
    float loss_percent;
    uint32_t bandwidth_bps;
    bool is_active;
    bool is_wifi;
};

class MultipathManager {
public:
    MultipathManager();
    ~MultipathManager();

    // Register a new network path (e.g., eth0, wlan0)
    void add_path(std::unique_ptr<ITransport> path, bool is_wifi);
    
    // Evaluate path metrics and select the best one
    void evaluate_paths();

    // Send packet via the current best path (Lowest Latency Mode)
    Result<void> send(std::span<const uint8_t> payload, uint8_t stream_id);
    
    // Receive from any active path
    Result<std::span<const uint8_t>> receive(uint32_t timeout_us);

    std::vector<PathStats> get_path_stats() const;

private:
    struct PathInfo {
        std::unique_ptr<ITransport> transport;
        PathStats stats;
        uint32_t score; // Lower is better
    };
    
    std::vector<PathInfo> paths_;
    size_t active_path_idx_{0};
    
    uint32_t calculate_score(const PathStats& stats);
};

} // namespace ud
