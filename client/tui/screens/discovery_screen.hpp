#pragma once

#include <ftxui/component/component.hpp>
#include <string>
#include <vector>

namespace ud {

struct DiscoveredServer {
    std::string name;
    std::string ip_address;
    uint32_t latency_ms;
    bool is_paired;
};

class DiscoveryScreen {
public:
    DiscoveryScreen();
    ~DiscoveryScreen() = default;

    // Triggered when mDNS discovers a new UltraDisplay server
    void add_server(const DiscoveredServer& server);
    
    // Returns the FTXUI component to be embedded in TuiApp
    ftxui::Component get_component();

    // Callback when user hits Enter on a server
    void on_connect_attempt(std::function<void(const DiscoveredServer&)> callback);

private:
    std::vector<DiscoveredServer> servers_;
    int selected_index_{0};
    std::function<void(const DiscoveredServer&)> connect_callback_;
    
    ftxui::Component renderer_;
    ftxui::Component build_ui();
};

} // namespace ud
