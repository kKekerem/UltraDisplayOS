#pragma once

#include <ftxui/screen/color.hpp>
#include <ftxui/dom/elements.hpp>

namespace ud::theme {

// Dark Theme Definitions (Appliance Default)
const ftxui::Color Background = ftxui::Color::RGB(10, 14, 26);   // #0A0E1A
const ftxui::Color Surface    = ftxui::Color::RGB(20, 25, 41);   // #141929
const ftxui::Color Border     = ftxui::Color::RGB(42, 49, 80);   // #2A3150
const ftxui::Color Primary    = ftxui::Color::RGB(59, 130, 246); // #3B82F6 (Electric Blue)
const ftxui::Color Accent     = ftxui::Color::RGB(6, 214, 160);  // #06D6A0 (Teal Green)
const ftxui::Color Text       = ftxui::Color::RGB(241, 245, 249); // #F1F5F9 (Near White)
const ftxui::Color TextDim    = ftxui::Color::RGB(100, 116, 139); // #64748B (Slate Gray)

// Status Colors
const ftxui::Color Success    = ftxui::Color::RGB(16, 185, 129); // #10B981
const ftxui::Color Warning    = ftxui::Color::RGB(245, 158, 11);  // #F59E0B
const ftxui::Color Error      = ftxui::Color::RGB(239, 68, 68);   // #EF4444

// UI Helper Decorators
inline ftxui::Decorator window_box() {
    return ftxui::border_styled(ftxui::ROUNDED) 
         | ftxui::bgcolor(Surface) 
         | ftxui::color(Text);
}

inline ftxui::Decorator primary_button() {
    return ftxui::bgcolor(Primary) | ftxui::color(ftxui::Color::White) | ftxui::bold;
}

inline ftxui::Color latency_color(uint32_t rtt_ms) {
    if (rtt_ms < 5) return Success;
    if (rtt_ms < 15) return Warning;
    return Error;
}

} // namespace ud::theme
