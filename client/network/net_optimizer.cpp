#include "net_optimizer.hpp"
#include <cstdlib>
#include <string>

namespace ud {

Result<void> NetOptimizer::apply_offloads(const char* interface_name) {
    // Using ethtool to enforce offloads. 
    // In a fully native C implementation, we would use SIOCETHTOOL ioctls directly.
    std::string cmd = std::string("ethtool -K ") + interface_name + 
                      " rx on tx on tso on gso on gro on";
    if (system(cmd.c_str()) != 0) {
        return Error(ErrorCode::SystemError, "Failed to apply ethtool offloads");
    }
    return Result<void>();
}

Result<void> NetOptimizer::auto_tune(const char* interface_name) {
    // Maximize ring buffers to avoid packet drops on high burst
    std::string cmd_ring = std::string("ethtool -G ") + interface_name + " rx 4096 tx 4096";
    system(cmd_ring.c_str()); // Ignore error, some NICs don't support 4096

    // Disable interrupt coalescing for lowest latency
    std::string cmd_coal = std::string("ethtool -C ") + interface_name + " rx-usecs 0 tx-usecs 0";
    system(cmd_coal.c_str());

    // Adjust txqueuelen via iproute2
    std::string cmd_ip = std::string("ip link set ") + interface_name + " txqueuelen 10000";
    system(cmd_ip.c_str());

    return Result<void>();
}

Result<void> NetOptimizer::enable_rps(const char* interface_name) {
    // Enable Receive Packet Steering across all cores
    std::string path = std::string("/sys/class/net/") + interface_name + "/queues/";
    
    // Simplistic loop for all rx queues
    for (int i = 0; i < 16; ++i) {
        std::string rps_path = path + "rx-" + std::to_string(i) + "/rps_cpus";
        std::string cmd = std::string("echo f > ") + rps_path + " 2>/dev/null";
        system(cmd.c_str());
    }
    
    return Result<void>();
}

} // namespace ud
