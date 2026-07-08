#pragma once

#include "shared/util/result.hpp"
#include <string>
#include <vector>

namespace ud {

struct GpuInfo {
    std::string name;
    std::string drm_node; // e.g. "/dev/dri/renderD128"
    bool is_integrated;
    uint32_t benchmark_score; // Higher is better
};

class MultiGpuManager {
public:
    MultiGpuManager();
    ~MultiGpuManager();

    // Probe the system for all available GPUs (Intel, AMD)
    Result<void> probe_hardware();

    // Returns a list of discovered GPUs, sorted by capability/preference
    std::vector<GpuInfo> get_available_gpus() const;

    // Automatically select the best GPU for decoding
    Result<GpuInfo> select_optimal_decoder_gpu() const;

    // Returns the DRM node that is physically connected to the active display
    Result<std::string> get_display_gpu_node() const;

private:
    std::vector<GpuInfo> gpus_;
    
    // Internal benchmark to determine which GPU can decode faster/better
    void benchmark_gpus();
};

} // namespace ud
