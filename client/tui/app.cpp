#include "app.hpp"
#include "theme.hpp"
#include "screens/wifi_screen.hpp"
#include <ftxui/component/event.hpp>

using namespace ftxui;

namespace ud {

TuiApp::TuiApp() : screen_(ScreenInteractive::Fullscreen()) {}
TuiApp::~TuiApp() = default;

Result<void> TuiApp::init() {
    // Mock up empty components for now
    home_screen_ = Renderer([] { return text("Home Screen") | center; });
    settings_screen_ = Renderer([] { return text("Settings") | center; });
    diagnostics_screen_ = Renderer([] { return text("Diagnostics") | center; });
    overlay_screen_ = Renderer([] { return text("Overlay Active [Press F1 to Close]") | center | theme::window_box(); });
    
    auto wifi = std::make_shared<WifiScreen>();
    wifi_screen_ = wifi->get_component();

    main_container_ = build_ui_tree();
    return Result<void>();
}

void TuiApp::run() {
    screen_.Loop(main_container_);
}

void TuiApp::exit() {
    screen_.Exit();
}

void TuiApp::set_overlay_mode(bool is_overlay) {
    is_overlay_mode_.store(is_overlay, std::memory_order_release);
    if (is_overlay) {
        current_screen_ = ScreenID::Overlay;
    } else {
        current_screen_ = ScreenID::Home;
    }
}

void TuiApp::navigate_to(ScreenID screen) {
    if (!is_overlay_mode_.load(std::memory_order_acquire)) {
        current_screen_ = screen;
    }
}

Component TuiApp::build_ui_tree() {
    auto main_renderer = Renderer([this] {
        if (is_overlay_mode_.load(std::memory_order_acquire)) {
            return overlay_screen_->Render() | center;
        }

        Element current_view;
        switch (current_screen_) {
            case ScreenID::Home: current_view = home_screen_->Render(); break;
            case ScreenID::Settings: current_view = settings_screen_->Render(); break;
            case ScreenID::Diagnostics: current_view = diagnostics_screen_->Render(); break;
            case ScreenID::Wifi: current_view = wifi_screen_->Render(); break;
            default: current_view = home_screen_->Render(); break;
        }

        return current_view | bgcolor(theme::Background);
    });

    // Handle global F1 keypress
    auto event_handler = CatchEvent(main_renderer, [this](Event e) {
        if (e == Event::F1) {
            bool current = is_overlay_mode_.load(std::memory_order_acquire);
            set_overlay_mode(!current);
            return true;
        }
        if (e == Event::F3) {
            navigate_to(ScreenID::Wifi);
            return true;
        }
        if (e == Event::Escape && !is_overlay_mode_.load(std::memory_order_acquire)) {
            navigate_to(ScreenID::Home);
            return true;
        }
        return false;
    });

    return event_handler;
}

} // namespace ud
