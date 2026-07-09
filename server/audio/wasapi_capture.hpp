#pragma once

#include "shared/util/result.hpp"
#include "shared/audio/audio_pipeline.hpp"
#include <memory>


#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace ud {

class WasapiCapture {
public:
    WasapiCapture();
    ~WasapiCapture();

    // Initializes loopback capture on the default output device
    Result<void> init();

    // Starts capture thread
    Result<void> start();
    void stop();

    // Fetch the latest block of PCM audio (float format)
    Result<std::vector<float>> read_frames();

    AudioFormat get_format() const { return format_; }

private:
    Microsoft::WRL::ComPtr<IMMDevice> device_;
    Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> capture_client_;
    
    AudioFormat format_{};
    HANDLE event_handle_{nullptr};
    
    bool is_capturing_{false};
};

} // namespace ud
