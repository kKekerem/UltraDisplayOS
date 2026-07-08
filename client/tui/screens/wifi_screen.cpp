#include "wifi_screen.hpp"
#include "client/tui/theme.hpp"
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <iostream>
#include <memory>
#include <array>
#include <cstdio>

using namespace ftxui;

namespace ud {

WifiScreen::WifiScreen() {
    renderer_ = build_ui();
    scan_networks();
}

void WifiScreen::scan_networks() {
    is_scanning_ = true;
    status_message_ = "Scanning for networks...";
    networks_.clear();

    // Run 'iw' command to list SSIDs (in a real app, do this asynchronously)
    std::array<char, 128> buffer;
    std::string result;
    // Note: Assuming wlan0 is the interface. Buildroot images usually name it wlan0 or mlan0.
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("iw dev wlan0 scan | grep SSID | cut -d ' ' -f 2-", "r"), pclose);
    
    if (pipe) {
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            std::string ssid = buffer.data();
            // Remove newline
            if (!ssid.empty() && ssid.back() == '\n') ssid.pop_back();
            // Filter duplicates and empties
            if (!ssid.empty() && std::find(networks_.begin(), networks_.end(), ssid) == networks_.end()) {
                networks_.push_back(ssid);
            }
        }
    }

    if (networks_.empty()) {
        networks_.push_back("No networks found. (Is Wi-Fi adapter present?)");
    }

    selected_index_ = 0;
    is_scanning_ = false;
    status_message_ = "Scan complete.";
}

void WifiScreen::connect_to_network(const std::string& ssid, const std::string& password) {
    status_message_ = "Connecting to " + ssid + "...";
    
    // Generate wpa_supplicant.conf
    std::string cmd = "wpa_passphrase \"" + ssid + "\" \"" + password + "\" > /tmp/wpa_supplicant.conf";
    system(cmd.c_str());

    // Kill existing wpa_supplicant and restart
    system("killall wpa_supplicant 2>/dev/null");
    system("wpa_supplicant -B -i wlan0 -c /tmp/wpa_supplicant.conf");
    
    // Request DHCP lease
    system("killall udhcpc 2>/dev/null");
    system("udhcpc -i wlan0 -b");

    status_message_ = "Connection script executed. Wait for IP...";
    password_input_.clear();
}

ftxui::Component WifiScreen::get_component() {
    return renderer_;
}

ftxui::Component WifiScreen::build_ui() {
    auto input_password = Input(&password_input_, "Enter password");

    auto connect_button = Button("Connect", [this] {
        if (!networks_.empty() && networks_[0].find("No networks") == std::string::npos) {
            connect_to_network(networks_[selected_index_], password_input_);
        }
    });

    auto scan_button = Button("Rescan", [this] {
        scan_networks();
    });

    auto container = Container::Vertical({
        input_password,
        Container::Horizontal({
            connect_button,
            scan_button
        })
    });

    auto renderer = Renderer(container, [this, container] {
        Elements network_elements;
        
        for (int i = 0; i < static_cast<int>(networks_.size()); ++i) {
            bool is_selected = (i == selected_index_);
            auto row = text(networks_[i]) | (is_selected ? bold : nothing) | color(is_selected ? theme::Text : theme::TextDim);
            if (is_selected) {
                row = row | bgcolor(theme::Border);
            }
            network_elements.push_back(row);
        }

        return vbox({
            text("Wi-Fi Setup") | bold | center | color(theme::Primary),
            separator(),
            vbox(network_elements) | border | size(HEIGHT, EQUAL, 10),
            separator(),
            hbox({ text("Password: "), container->Render() | flex }),
            separator(),
            text(status_message_) | color(theme::Warning) | center,
            text("Use ARROWS to select network, TAB to focus password/buttons") | color(theme::TextDim) | center
        }) | theme::window_box() | size(WIDTH, GREATER_THAN, 50);
    });

    auto event_handler = CatchEvent(renderer, [this](Event e) {
        if (e == Event::ArrowUp && selected_index_ > 0) {
            selected_index_--;
            return true;
        }
        if (e == Event::ArrowDown && selected_index_ < static_cast<int>(networks_.size() - 1)) {
            selected_index_++;
            return true;
        }
        return false;
    });

    return event_handler;
}

} // namespace ud
