#pragma once

#include "shared/util/clock.hpp"
#include "client/decoder/vaapi_decoder.hpp"
#include "client/audio/alsa_playback.hpp"
#include <memory>

namespace ud {

class SyncEngine {
public:
    SyncEngine(std::shared_ptr<AlsaPlayback> audio, const ClockSync& clock_sync);
    ~SyncEngine() = default;

    // Called when a new video frame is fully decoded.
    // Calculates exactly when to commit the atomic page flip based on VRR capability and audio PTS.
    void schedule_presentation(const DecodedFrame& frame);

    // Determines if the audio playback rate needs to be slightly tweaked to prevent lip-sync drift.
    void evaluate_drift();

private:
    std::shared_ptr<AlsaPlayback> audio_;
    const ClockSync& clock_sync_;
    
    int64_t target_presentation_offset_us_{0}; // Desired buffer depth (e.g. 10ms)
};

} // namespace ud
