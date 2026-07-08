#include "hw_optimizer.hpp"
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

namespace ud {

bool HardwareOptimizer::is_locked_ = false;

Result<void> HardwareOptimizer::init() {
    // Basic init, checks if running as root
    if (geteuid() != 0) {
        return Error(ErrorCode::SystemError, "HardwareOptimizer requires root privileges");
    }
    return Result<void>();
}

void HardwareOptimizer::lock_performance_state(bool locked) {
    if (is_locked_ == locked) return;
    
    std::string governor = locked ? "performance" : "powersave";
    
    DIR* dir = opendir("/sys/devices/system/cpu/");
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        if (name.find("cpu") == 0 && isdigit(name[3])) {
            std::string gov_path = "/sys/devices/system/cpu/" + name + "/cpufreq/scaling_governor";
            std::ofstream f(gov_path);
            if (f.is_open()) {
                f << governor;
            }
        }
    }
    closedir(dir);

    // Disable CPU C-States deep sleep (C1E, C3, C6) if locking for max perf
    std::ofstream cstate_file("/dev/cpu_dma_latency");
    static int cstate_fd = -1;
    
    if (locked) {
        if (cstate_fd < 0) {
            cstate_fd = open("/dev/cpu_dma_latency", O_RDWR);
            if (cstate_fd >= 0) {
                int32_t target_latency = 0; // 0 disables all deep sleep states
                write(cstate_fd, &target_latency, sizeof(target_latency));
            }
        }
    } else {
        if (cstate_fd >= 0) {
            close(cstate_fd);
            cstate_fd = -1;
        }
    }
    
    is_locked_ = locked;
}

void HardwareOptimizer::bind_nic_interrupts(const char* interface_name, int core_id) {
    // Find IRQs for the given network interface and set smp_affinity
    // /sys/class/net/<interface>/device/msi_irqs/
    std::string path = std::string("/sys/class/net/") + interface_name + "/device/msi_irqs/";
    DIR* dir = opendir(path.c_str());
    if (!dir) return;

    int mask = (1 << core_id);
    char hex_mask[16];
    snprintf(hex_mask, sizeof(hex_mask), "%x", mask);

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        
        std::string irq_path = std::string("/proc/irq/") + entry->d_name + "/smp_affinity";
        std::ofstream f(irq_path);
        if (f.is_open()) {
            f << hex_mask;
        }
    }
    closedir(dir);
}

void HardwareOptimizer::lock_gpu_clocks(bool locked) {
    // For Intel GPUs
    std::string pstate_path = "/sys/class/drm/card0/gt_min_freq_mhz";
    std::string max_pstate_path = "/sys/class/drm/card0/gt_max_freq_mhz";
    
    std::ifstream max_f(max_pstate_path);
    if (max_f.is_open()) {
        std::string max_freq;
        max_f >> max_freq;
        
        std::ofstream min_f(pstate_path);
        if (min_f.is_open()) {
            if (locked) {
                min_f << max_freq; // Force min to max
            } else {
                min_f << "300"; // Reset to baseline
            }
        }
    }
}

} // namespace ud
