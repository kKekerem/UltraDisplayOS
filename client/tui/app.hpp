#pragma once

#include "shared/util/result.hpp"
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <memory>
#include <atomic>

namespace ud {

enum class ScreenID {
    Home,
    Settings,
    Diagnostics,
    Wifi,
    Pairing,
    Overlay // The F1 transparent overlay shown during streaming
};

class TuiApp {
public:
    TuiApp();
    ~TuiApp();

    Result<void> init();
    
    // Runs the main FTXUI event loop on the current thread
    void run();

    // Requests the application to exit safely
    void exit();

    // Switch between full-screen TUI (Home) and Stream Overlay mode
    void set_overlay_mode(bool is_overlay);

    // Navigates between internal TUI screens
    void navigate_to(ScreenID screen);

private:
    std::shared_ptr<ftxui::ScreenInteractive> screen_;
    ftxui::Component main_container_;
    
    std::atomic<bool> is_overlay_mode_{false};
    ScreenID current_screen_{ScreenID::Home};

    // Components
    ftxui::Component home_screen_;
    ftxui::Component settings_screen_;
    ftxui::Component diagnostics_screen_;
    ftxui::Component overlay_screen_;
    ftxui::Component wifi_screen_; // Added Wi-Fi Screen

    ftxui::Component build_ui_tree();
};

} // namespace ud
