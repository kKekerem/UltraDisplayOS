#pragma once

#include "shared/util/result.hpp"
#include "shared/audio/audio_pipeline.hpp"
#include <alsa/asoundlib.h>
#include <span>

namespace ud {

class AlsaPlayback {
public:
    AlsaPlayback();
    ~AlsaPlayback();

    // Initializes ALSA using direct hardware access (e.g. "hw:0,0") bypassing PulseAudio/PipeWire
    Result<void> init(const AudioFormat& format, const char* device_name = "default");

    // Write decoded PCM float frames to the audio device
    Result<void> write_frames(std::span<const float> pcm_data);

    // Dynamic lip-sync compensation. Adjusts playback rate to match video presentation.
    void adjust_playback_rate(float rate_multiplier);

private:
    snd_pcm_t* pcm_handle_{nullptr};
    AudioFormat format_{};
    
    Result<void> setup_hw_params();
};

} // namespace ud
