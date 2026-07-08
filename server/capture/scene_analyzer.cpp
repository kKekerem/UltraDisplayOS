#include "scene_analyzer.hpp"
#include <algorithm>

namespace ud {

SceneAnalyzer::SceneAnalyzer() = default;
SceneAnalyzer::~SceneAnalyzer() = default;

void SceneAnalyzer::set_keyframe_threshold(float percent) {
    keyframe_threshold_ = percent;
}

SceneAnalysis SceneAnalyzer::analyze(uint32_t frame_width, uint32_t frame_height, const std::vector<RECT>& dirty_rects) {
    SceneAnalysis result;
    
    if (dirty_rects.empty()) {
        result.dirty_percent = 0.0f;
        result.is_idle = true;
        result.trigger_keyframe = false;
        return result;
    }

    result.merged_rects = merge_rects(dirty_rects);
    
    uint64_t total_area = static_cast<uint64_t>(frame_width) * frame_height;
    uint64_t dirty_area = calculate_area(result.merged_rects);

    result.dirty_percent = (static_cast<float>(dirty_area) / static_cast<float>(total_area)) * 100.0f;
    result.is_idle = (result.dirty_percent < 0.01f);
    result.trigger_keyframe = (result.dirty_percent >= keyframe_threshold_);

    return result;
}

std::vector<RECT> SceneAnalyzer::merge_rects(const std::vector<RECT>& input) {
    if (input.empty()) return {};

    std::vector<RECT> merged;
    merged.reserve(input.size());

    for (const auto& rect : input) {
        bool absorbed = false;
        for (auto& m : merged) {
            if (rects_intersect(m, rect)) {
                m = merge(m, rect);
                absorbed = true;
                break;
            }
        }
        if (!absorbed) {
            merged.push_back(rect);
        }
    }

    // Second pass to merge newly overlapping rects
    bool changed;
    do {
        changed = false;
        for (size_t i = 0; i < merged.size(); ++i) {
            for (size_t j = i + 1; j < merged.size(); ) {
                if (rects_intersect(merged[i], merged[j])) {
                    merged[i] = merge(merged[i], merged[j]);
                    merged.erase(merged.begin() + j);
                    changed = true;
                } else {
                    ++j;
                }
            }
        }
    } while (changed);

    return merged;
}

bool SceneAnalyzer::rects_intersect(const RECT& a, const RECT& b) const {
    return !(a.right <= b.left || 
             a.left >= b.right || 
             a.bottom <= b.top || 
             a.top >= b.bottom);
}

RECT SceneAnalyzer::merge(const RECT& a, const RECT& b) const {
    return RECT{
        std::min(a.left, b.left),
        std::min(a.top, b.top),
        std::max(a.right, b.right),
        std::max(a.bottom, b.bottom)
    };
}

uint64_t SceneAnalyzer::calculate_area(const std::vector<RECT>& rects) const {
    uint64_t area = 0;
    for (const auto& r : rects) {
        uint64_t width = r.right - r.left;
        uint64_t height = r.bottom - r.top;
        area += width * height;
    }
    return area;
}

} // namespace ud
