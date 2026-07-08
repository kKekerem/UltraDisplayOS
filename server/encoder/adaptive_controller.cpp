#include "adaptive_controller.hpp"
#include "shared/util/log.hpp"
#include <algorithm>

namespace ud {

AdaptiveController::AdaptiveController() = default;
AdaptiveController::AdaptiveController::~AdaptiveController() = default;

void AdaptiveController::init(const EncoderCapabilities& server_caps, const DecoderCapabilities& client_caps) {
    server_caps_ = server_caps;
    client_caps_ = client_caps;

    current_config_.codec = VideoCodec::H264; // Safe default
    current_config_.bitrate_bps = 5'000'000;  // 5 Mbps default
    current_config_.framerate = 60;
    current_config_.force_idr = true;
    current_config_.enable_lossless = false;
    current_config_.roi_qp_delta = 15; // Moderate suppression for clean areas

    adapt_codec();
}

void AdaptiveController::update_network_stats(const TransportStats& stats) {
    last_stats_ = stats;
    adapt_bitrate();
    adapt_codec();
}

EncoderConfig AdaptiveController::evaluate_frame(const SceneAnalysis& scene) {
    current_config_.force_idr = false;
    frames_since_keyframe_++;

    if (scene.trigger_keyframe || frames_since_keyframe_ >= max_gop_size_) {
        current_config_.force_idr = true;
        frames_since_keyframe_ = 0;
        UD_LOG_DEBUG("encoder", "Triggering IDR. Scene change: {}, GOP limit: {}", 
                     scene.trigger_keyframe, frames_since_keyframe_ >= max_gop_size_);
    }

    if (scene.is_idle) {
        // Suppress bitrate heavily if nothing changed
        current_config_.roi_qp_delta = 51; 
    } else {
        current_config_.roi_qp_delta = 20; // Normal background suppression
    }

    return current_config_;
}

void AdaptiveController::adapt_bitrate() {
    // Basic GCC-style rate control wrapper.
    // In production, this heavily relies on congestion_gcc.cpp output
    if (last_stats_.bandwidth_est_bps > 0) {
        // Target 80% of estimated bandwidth to avoid queuing delay
        uint32_t target_bps = static_cast<uint32_t>(last_stats_.bandwidth_est_bps * 0.8f);
        
        // Clamp to sane values
        target_bps = std::clamp(target_bps, 1'000'000u, 150'000'000u);
        
        if (std::abs(static_cast<int>(target_bps) - static_cast<int>(current_config_.bitrate_bps)) > 1'000'000) {
            current_config_.bitrate_bps = target_bps;
            UD_LOG_INFO("encoder", "Bitrate adapted to {} Mbps", target_bps / 1'000'000);
        }
    }
}

void AdaptiveController::adapt_codec() {
    // Logic for Lossless vs AV1 vs HEVC vs H264
    // Requires bandwidth > 2 Gbps for lossless
    if (last_stats_.bandwidth_est_bps > 2'000'000'000ULL && 
        server_caps_.supports_lossless && client_caps_.supports_lossless) {
        if (!current_config_.enable_lossless) {
            current_config_.enable_lossless = true;
            UD_LOG_INFO("encoder", "10G LAN detected. Switching to Lossless Mode.");
        }
    } else {
        current_config_.enable_lossless = false;
        
        uint32_t common_mask = server_caps_.supported_codecs_mask & client_caps_.supported_codecs_mask;
        VideoCodec next_codec = VideoCodec::H264;

        if (common_mask & (1 << static_cast<uint8_t>(VideoCodec::AV1))) {
            next_codec = VideoCodec::AV1;
        } else if (common_mask & (1 << static_cast<uint8_t>(VideoCodec::HEVC))) {
            next_codec = VideoCodec::HEVC;
        }

        if (current_config_.codec != next_codec) {
            current_config_.codec = next_codec;
            current_config_.force_idr = true; // Force IDR on codec switch
            UD_LOG_INFO("encoder", "Codec adapted to {}", static_cast<int>(next_codec));
        }
    }
}

} // namespace ud
