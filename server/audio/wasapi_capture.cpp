#include "server/audio/wasapi_capture.hpp"
#include <iostream>
#include <stdexcept>
#include <Functiondiscoverykeys_devpkey.h>

#pragma comment(lib, "ole32.lib")

namespace ud {

WasapiCapture::WasapiCapture() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
}

WasapiCapture::~WasapiCapture() {
    stop();
    CoUninitialize();
}

Result<void> WasapiCapture::init() {
    HRESULT hr;

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) return Result<void>::create_error("Failed to create IMMDeviceEnumerator");

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    if (FAILED(hr)) return Result<void>::create_error("Failed to get default audio endpoint");

    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audio_client_);
    if (FAILED(hr)) return Result<void>::create_error("Failed to activate IAudioClient");

    WAVEFORMATEX* mix_format = nullptr;
    hr = audio_client_->GetMixFormat(&mix_format);
    if (FAILED(hr)) return Result<void>::create_error("Failed to get mix format");

    if (mix_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix_format);
        if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            format_.bit_depth = 32;
        } else if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
            format_.bit_depth = mix_format->wBitsPerSample;
        } else {
            CoTaskMemFree(mix_format);
            return Result<void>::create_error("Unsupported audio format");
        }
    } else {
        format_.bit_depth = mix_format->wBitsPerSample;
    }

    format_.sample_rate = mix_format->nSamplesPerSec;
    format_.channels = mix_format->nChannels;

    // Loopback capture
    const REFERENCE_TIME requested_duration = 10000000; // 1 second buffer
    hr = audio_client_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        requested_duration,
        0,
        mix_format,
        nullptr
    );

    if (FAILED(hr)) {
        CoTaskMemFree(mix_format);
        return Result<void>::create_error("Failed to initialize IAudioClient for loopback");
    }
    CoTaskMemFree(mix_format);

    event_handle_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!event_handle_) return Result<void>::create_error("Failed to create event handle");

    hr = audio_client_->SetEventHandle(event_handle_);
    if (FAILED(hr)) return Result<void>::create_error("Failed to set event handle");

    hr = audio_client_->GetService(IID_PPV_ARGS(&capture_client_));
    if (FAILED(hr)) return Result<void>::create_error("Failed to get IAudioCaptureClient service");

    return Result<void>::create_success();
}

Result<void> WasapiCapture::start() {
    if (is_capturing_) return Result<void>::create_success();
    HRESULT hr = audio_client_->Start();
    if (FAILED(hr)) return Result<void>::create_error("Failed to start audio client");
    is_capturing_ = true;
    return Result<void>::create_success();
}

void WasapiCapture::stop() {
    if (is_capturing_ && audio_client_) {
        audio_client_->Stop();
        is_capturing_ = false;
    }
    if (event_handle_) {
        CloseHandle(event_handle_);
        event_handle_ = nullptr;
    }
}

Result<std::vector<float>> WasapiCapture::read_frames() {
    if (!is_capturing_) return Result<std::vector<float>>::create_error("Not capturing");

    DWORD wait_res = WaitForSingleObject(event_handle_, 1000);
    if (wait_res != WAIT_OBJECT_0) return Result<std::vector<float>>::create_error("Wait for audio event failed");

    UINT32 packet_length = 0;
    HRESULT hr = capture_client_->GetNextPacketSize(&packet_length);
    if (FAILED(hr)) return Result<std::vector<float>>::create_error("Failed to get next packet size");

    std::vector<float> frames;
    while (packet_length != 0) {
        BYTE* data = nullptr;
        UINT32 num_frames_available = 0;
        DWORD flags = 0;

        hr = capture_client_->GetBuffer(&data, &num_frames_available, &flags, nullptr, nullptr);
        if (FAILED(hr)) break;

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
            frames.insert(frames.end(), num_frames_available * format_.channels, 0.0f);
        } else {
            if (format_.bit_depth == 32) {
                float* f_data = reinterpret_cast<float*>(data);
                frames.insert(frames.end(), f_data, f_data + num_frames_available * format_.channels);
            } else if (format_.bit_depth == 16) {
                int16_t* s_data = reinterpret_cast<int16_t*>(data);
                for (UINT32 i = 0; i < num_frames_available * format_.channels; ++i) {
                    frames.push_back(s_data[i] / 32768.0f);
                }
            }
        }

        hr = capture_client_->ReleaseBuffer(num_frames_available);
        if (FAILED(hr)) break;

        hr = capture_client_->GetNextPacketSize(&packet_length);
        if (FAILED(hr)) break;
    }

    return Result<std::vector<float>>::create_success(std::move(frames));
}

} // namespace ud
