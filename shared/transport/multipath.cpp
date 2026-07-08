#include "multipath.hpp"
#include <algorithm>

namespace ud {

MultipathManager::MultipathManager() {}
MultipathManager::~MultipathManager() = default;

void MultipathManager::add_path(std::unique_ptr<ITransport> path, bool is_wifi) {
    PathInfo info;
    info.transport = std::move(path);
    info.stats = {0, 0.0f, 0, true, is_wifi};
    info.score = 0;
    paths_.push_back(std::move(info));
    evaluate_paths();
}

uint32_t MultipathManager::calculate_score(const PathStats& stats) {
    if (!stats.is_active) return 0xFFFFFFFF; // Max score
    
    // Score heavily weights RTT. Loss adds massive penalty.
    uint32_t score = stats.rtt_us;
    score += static_cast<uint32_t>(stats.loss_percent * 10000.0f); // 1% loss = 10ms penalty
    
    // Prefer ethernet over wifi if similar latency
    if (stats.is_wifi) {
        score += 2000; // 2ms base penalty for wifi jitter
    }
    
    return score;
}

void MultipathManager::evaluate_paths() {
    if (paths_.empty()) return;

    uint32_t best_score = 0xFFFFFFFF;
    size_t best_idx = 0;

    for (size_t i = 0; i < paths_.size(); ++i) {
        auto stats = paths_[i].transport->stats();
        paths_[i].stats.rtt_us = stats.rtt_us;
        paths_[i].stats.loss_percent = stats.packet_loss_percent;
        paths_[i].stats.bandwidth_bps = stats.bandwidth_est_bps;

        paths_[i].score = calculate_score(paths_[i].stats);
        if (paths_[i].score < best_score) {
            best_score = paths_[i].score;
            best_idx = i;
        }
    }

    active_path_idx_ = best_idx;
}

Result<void> MultipathManager::send(std::span<const uint8_t> payload, uint8_t stream_id) {
    if (paths_.empty()) {
        return Error(ErrorCode::SystemError, "No paths available");
    }
    
    // Always use active path for latency, optionally replicate if multipath fully active
    return paths_[active_path_idx_].transport->send(payload, stream_id);
}

Result<std::span<const uint8_t>> MultipathManager::receive(uint32_t timeout_us) {
    if (paths_.empty()) {
        return Error(ErrorCode::SystemError, "No paths available");
    }
    
    // Round robin or polling all active paths. For simplicity we check the best path first.
    // In a true async system, we'd wait on an epoll/IOCP set of all paths.
    // Given the abstraction, we poll the active path.
    auto res = paths_[active_path_idx_].transport->receive(timeout_us);
    if (res.has_value()) {
        return res;
    }
    
    // Fallback to check other paths with 0 timeout if the primary failed (e.g. timeout)
    for (size_t i = 0; i < paths_.size(); ++i) {
        if (i == active_path_idx_) continue;
        auto alt_res = paths_[i].transport->receive(0);
        if (alt_res.has_value()) {
            return alt_res;
        }
    }
    
    return res; // Return original error
}

std::vector<PathStats> MultipathManager::get_path_stats() const {
    std::vector<PathStats> stats_out;
    stats_out.reserve(paths_.size());
    for (const auto& path : paths_) {
        stats_out.push_back(path.stats);
    }
    return stats_out;
}

} // namespace ud
