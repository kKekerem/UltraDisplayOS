#pragma once

#include "shared/util/result.hpp"

namespace ud {

class NetOptimizer {
public:
    // Applies ethtool offloads (GRO, GSO, TSO) and tuning to the specified interface
    static Result<void> apply_offloads(const char* interface_name);

    // Automatically tunes ring buffers and interrupt coalescing based on link speed
    static Result<void> auto_tune(const char* interface_name);

    // Enables Receive Packet Steering (RPS) to distribute load across CPU cores
    static Result<void> enable_rps(const char* interface_name);
};

} // namespace ud
