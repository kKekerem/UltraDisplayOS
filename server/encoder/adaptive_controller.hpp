#pragma once

#include "shared/protocol/codec_config.hpp"
#include "shared/transport/transport.hpp"
#include "server/capture/scene_analyzer.hpp"
#include <memory>

namespace ud {

struct EncoderConfig {
    VideoCodec codec;
    uint32_t bitrate_bps;
    uint32_t framerate;
    bool force_idr;
    bool enable_lossless;
    uint32_t roi_qp_delta; // Used for clean regions
};

class AdaptiveController {
public:
    AdaptiveController();
    ~AdaptiveController();

    // Set initial capabilities from handshake
    void init(const EncoderCapabilities& server_caps, const DecoderCapabilities& client_caps);

    // Called periodically with network stats
    void update_network_stats(const TransportStats& stats);

    // Called every frame to determine encoder settings
    EncoderConfig evaluate_frame(const SceneAnalysis& scene);

    // Returns the currently selected ideal codec
    VideoCodec current_codec() const { return current_config_.codec; }

private:
    EncoderCapabilities server_caps_;
    DecoderCapabilities client_caps_;
    TransportStats last_stats_{};
    EncoderConfig current_config_{};
    
    uint32_t frames_since_keyframe_{0};
    uint32_t max_gop_size_{300}; // 5 seconds at 60fps

    void adapt_bitrate();
    void adapt_codec();
};

} // namespace ud
