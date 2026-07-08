#include "client/compositor/sync_engine.hpp"
#include <iostream>
#include <cmath>

namespace ud {

SyncEngine::SyncEngine(std::shared_ptr<AlsaPlayback> audio, const ClockSync& clock_sync)
    : audio_(std::move(audio)), clock_sync_(clock_sync) {
    target_presentation_offset_us_ = 10000; // 10ms default
}

void SyncEngine::schedule_presentation(const DecodedFrame& frame) {
    // 1. Get current remote time prediction from clock sync
    int64_t current_remote_time = clock_sync_.local_to_remote(ud::now_us());

    // 2. We want to present when the frame's PTS is reached (plus target buffer offset)
    int64_t presentation_time = frame.pts_us + target_presentation_offset_us_;

    // 3. Calculate wait time
    int64_t wait_us = presentation_time - current_remote_time;

    if (wait_us > 0) {
        // Here we would interact with the DRM stack to schedule an atomic page flip.
        // For demonstration, we simulate the wait. In real life we'd use a timerfd
        // to wake up and commit the DRM atomic request.
        // Sleep is not ideal for exact precision, but suffices for the placeholder logic
        // std::this_thread::sleep_for(std::chrono::microseconds(wait_us));
    } else {
        // Frame is late, present immediately!
    }

    // Call evaluate_drift periodically
    static int frame_count = 0;
    if (++frame_count % 60 == 0) {
        evaluate_drift();
    }
}

void SyncEngine::evaluate_drift() {
    if (!audio_) return;

    // This compares audio buffer depth vs video presentation time.
    // If audio is playing out faster than video arrives, slow down audio.
    // If audio is piling up, speed up audio.
    
    // Simplistic mock-up:
    // int64_t audio_pts = get_current_audio_pts();
    // int64_t video_pts = get_current_video_pts();
    // int64_t drift = audio_pts - video_pts;
    
    // float rate = 1.0f;
    // if (drift > 5000) rate = 0.99f;
    // if (drift < -5000) rate = 1.01f;
    
    // audio_->adjust_playback_rate(rate);
}

} // namespace ud
