#include "connection_screen.hpp"
#include "client/tui/theme.hpp"
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace ud {

ConnectionOverlay::ConnectionOverlay() {
    latency_history_.resize(60, 0); // 60 seconds of history
    renderer_ = build_ui();
}

void ConnectionOverlay::update_stats(const TransportStats& stats, uint32_t current_fps, const std::string& codec_info) {
    stats_ = stats;
    fps_ = current_fps;
    codec_info_ = codec_info;

    // Shift latency history
    for (size_t i = 0; i < latency_history_.size() - 1; ++i) {
        latency_history_[i] = latency_history_[i + 1];
    }
    latency_history_.back() = stats_.rtt_us / 1000; // Convert to ms
}

ftxui::Component ConnectionOverlay::get_component() {
    return renderer_;
}

ftxui::Component ConnectionOverlay::build_ui() {
    return Renderer([this] {
        float mbps = (stats_.bandwidth_est_bps / 1000000.0f);
        uint32_t rtt_ms = stats_.rtt_us / 1000;
        
        auto latency_text = text(std::to_string(rtt_ms) + " ms") | color(theme::latency_color(rtt_ms));
        
        auto stats_box = vbox({
            text("UltraDisplay Performance Metrics") | bold | color(theme::Primary) | center,
            separator(),
            hbox({
                vbox({
                    text("Latency:") | color(theme::TextDim),
                    text("Bandwidth:") | color(theme::TextDim),
                    text("Packet Loss:") | color(theme::TextDim)
                }) | size(WIDTH, GREATER_THAN, 15),
                vbox({
                    latency_text,
                    text(std::to_string(mbps).substr(0, 5) + " Mbps") | color(theme::Text),
                    text(std::to_string(stats_.packet_loss_percent).substr(0, 4) + " %") | 
                         color(stats_.packet_loss_percent > 1.0f ? theme::Error : theme::Success)
                })
            }),
            separator(),
            hbox({
                text("Video:") | color(theme::TextDim),
                text(" " + codec_info_ + " @ " + std::to_string(fps_) + " FPS") | color(theme::Text)
            }),
            separator(),
            text("Latency History") | color(theme::TextDim) | center,
            graph([this](int width, int height) {
                std::vector<int> output(width, 0);
                int step = std::max(1, (int)latency_history_.size() / width);
                for (int i = 0; i < width; ++i) {
                    int idx = (i * step) % latency_history_.size();
                    output[i] = latency_history_[idx];
                }
                return output;
            }) | color(theme::Accent) | size(HEIGHT, EQUAL, 5)
        }) | theme::window_box() | size(WIDTH, EQUAL, 45);

        return stats_box;
    });
}

} // namespace ud
