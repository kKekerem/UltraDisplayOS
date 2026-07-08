#pragma once

#include <ftxui/component/component.hpp>
#include <string>
#include <vector>

namespace ud {

class WifiScreen {
public:
    WifiScreen();
    ~WifiScreen() = default;

    // Trigger a system Wi-Fi scan
    void scan_networks();

    // Try connecting to the selected network
    void connect_to_network(const std::string& ssid, const std::string& password);

    // Returns the FTXUI component
    ftxui::Component get_component();

private:
    std::vector<std::string> networks_;
    int selected_index_{0};
    
    std::string password_input_;
    bool is_scanning_{false};
    std::string status_message_{"Ready."};

    ftxui::Component renderer_;
    ftxui::Component build_ui();
};

} // namespace ud
