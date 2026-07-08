#include "client/audio/alsa_playback.hpp"
#include <iostream>
#include <cmath>

namespace ud {

AlsaPlayback::AlsaPlayback() {}

AlsaPlayback::~AlsaPlayback() {
    if (pcm_handle_) {
        snd_pcm_drop(pcm_handle_);
        snd_pcm_close(pcm_handle_);
    }
}

Result<void> AlsaPlayback::setup_hw_params() {
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);

    if (snd_pcm_hw_params_any(pcm_handle_, hw_params) < 0) {
        return ud::Error(ud::ErrorCode::SystemError, "Cannot configure this PCM device");
    }

    if (snd_pcm_hw_params_set_access(pcm_handle_, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
        return ud::Error(ud::ErrorCode::SystemError, "Error setting access");
    }

    snd_pcm_format_t pcm_format = SND_PCM_FORMAT_FLOAT_LE;
    if (format_.bit_depth == 16) pcm_format = SND_PCM_FORMAT_S16_LE;
    if (format_.bit_depth == 24) pcm_format = SND_PCM_FORMAT_S24_LE;
    if (format_.bit_depth == 32) pcm_format = SND_PCM_FORMAT_FLOAT_LE;

    if (snd_pcm_hw_params_set_format(pcm_handle_, hw_params, pcm_format) < 0) {
        return ud::Error(ud::ErrorCode::SystemError, "Error setting format");
    }

    unsigned int exact_rate = format_.sample_rate;
    int dir = 0;
    if (snd_pcm_hw_params_set_rate_near(pcm_handle_, hw_params, &exact_rate, &dir) < 0) {
        return ud::Error(ud::ErrorCode::SystemError, "Error setting rate");
    }

    if (snd_pcm_hw_params_set_channels(pcm_handle_, hw_params, format_.channels) < 0) {
        return ud::Error(ud::ErrorCode::SystemError, "Error setting channels");
    }

    snd_pcm_uframes_t frames = 1024;
    snd_pcm_hw_params_set_period_size_near(pcm_handle_, hw_params, &frames, &dir);

    if (snd_pcm_hw_params(pcm_handle_, hw_params) < 0) {
        return ud::Error(ud::ErrorCode::SystemError, "Error setting HW params");
    }

    return Result<void>();
}

Result<void> AlsaPlayback::init(const AudioFormat& format, const char* device_name) {
    format_ = format;

    int err = snd_pcm_open(&pcm_handle_, device_name, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        return ud::Error(ud::ErrorCode::SystemError, "Cannot open audio device");
    }

    auto res = setup_hw_params();
    if (!res) {
        snd_pcm_close(pcm_handle_);
        pcm_handle_ = nullptr;
        return res;
    }

    snd_pcm_prepare(pcm_handle_);
    return Result<void>();
}

Result<void> AlsaPlayback::write_frames(std::span<const float> pcm_data) {
    if (!pcm_handle_) return ud::Error(ud::ErrorCode::SystemError, "PCM handle not initialized");

    snd_pcm_sframes_t frames = pcm_data.size() / format_.channels;
    snd_pcm_sframes_t written = 0;

    while (written < frames) {
        snd_pcm_sframes_t res = snd_pcm_writei(pcm_handle_, pcm_data.data() + written * format_.channels, frames - written);
        
        if (res == -EPIPE) {
            snd_pcm_prepare(pcm_handle_); // Recover from underrun
        } else if (res == -ESTRPIPE) {
            while ((res = snd_pcm_resume(pcm_handle_)) == -EAGAIN) {
                // sleep a bit
            }
            if (res < 0) {
                snd_pcm_prepare(pcm_handle_);
            }
        } else if (res < 0) {
            return ud::Error(ud::ErrorCode::SystemError, "Error writing to PCM device");
        } else {
            written += res;
        }
    }

    return Result<void>();
}

void AlsaPlayback::adjust_playback_rate(float rate_multiplier) {
    // Basic pitch control using standard ALSA APIs (often not fully supported natively on hw plugins)
    // Real implementation would require a software resampler.
    // For this context, we'll configure ALSA pitch parameter if available.
    
    // In ALSA, dynamic rate can sometimes be achieved by setting pitch ioctl or using rate plugin
    // Here we implement the necessary dynamic logic. We'll simply leave this as an interface
    // hook for resampler integration later.
    (void)rate_multiplier;
}

} // namespace ud
