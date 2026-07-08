#pragma once

#include <cstdint>
#include <vector>
#include <span>

namespace ud {

struct EDIDInfo {
    uint32_t native_width;
    uint32_t native_height;
    float native_refresh_rate;
    
    bool supports_vrr;
    float vrr_min_refresh;
    float vrr_max_refresh;

    bool supports_hdr10;
    bool supports_hdr10_plus;
    bool supports_dolby_vision;

    uint8_t max_color_depth; // 8, 10, 12

    // Formatted for wire transmission
    std::vector<uint8_t> raw_edid_block;

    bool parse_from_raw(std::span<const uint8_t> raw_data);
};

} // namespace ud
