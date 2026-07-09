#pragma once

#include <vector>
#include <cstdint>


#include <windows.h>

namespace ud {

struct SceneAnalysis {
    float dirty_percent;       // 0.0 to 100.0
    bool trigger_keyframe;     // True if dirty_percent > 80%
    bool is_idle;              // True if dirty_percent == 0.0
    std::vector<RECT> merged_rects; // Overlapping rects merged
};

class SceneAnalyzer {
public:
    SceneAnalyzer();
    ~SceneAnalyzer();

    // Set configuration thresholds
    void set_keyframe_threshold(float percent = 80.0f);

    // Analyze dirty rects from DXGI
    SceneAnalysis analyze(uint32_t frame_width, uint32_t frame_height, const std::vector<RECT>& dirty_rects);

private:
    float keyframe_threshold_{80.0f};

    std::vector<RECT> merge_rects(const std::vector<RECT>& input);
    bool rects_intersect(const RECT& a, const RECT& b) const;
    RECT merge(const RECT& a, const RECT& b) const;
    uint64_t calculate_area(const std::vector<RECT>& rects) const;
};

} // namespace ud
