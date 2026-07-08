#pragma once

#include "shared/util/result.hpp"

namespace ud {

class HardwareOptimizer {
public:
    static Result<void> init();

    // Forces CPU into maximum performance state (disables C-states, sets performance governor)
    static void lock_performance_state(bool locked);

    // Binds the networking interrupt queue to a specific CPU core for maximum throughput
    static void bind_nic_interrupts(const char* interface_name, int core_id);

    // Prevents the GPU from downclocking during stream
    static void lock_gpu_clocks(bool locked);

private:
    static bool is_locked_;
};

} // namespace ud
