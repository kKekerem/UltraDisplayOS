#include "discovery_screen.hpp"
#include "client/tui/theme.hpp"
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace ud {

DiscoveryScreen::DiscoveryScreen() {
    renderer_ = build_ui();
}

void DiscoveryScreen::add_server(const DiscoveredServer& server) {
    servers_.push_back(server);
}

void DiscoveryScreen::on_connect_attempt(std::function<void(const DiscoveredServer&)> callback) {
    connect_callback_ = std::move(callback);
}

ftxui::Component DiscoveryScreen::get_component() {
    return renderer_;
}

ftxui::Component DiscoveryScreen::build_ui() {
    auto menu_renderer = Renderer([this] {
        Elements elements;
        
        elements.push_back(
            text("UltraDisplay Servers") | bold | center | color(theme::Primary)
        );
        elements.push_back(separator());

        if (servers_.empty()) {
            elements.push_back(
                text("Searching for nearby servers on the local network...") | color(theme::TextDim) | center
            );
            // Mock spinning animation would go here in a real FTXUI loop
        } else {
            for (int i = 0; i < static_cast<int>(servers_.size()); ++i) {
                const auto& srv = servers_[i];
                bool is_selected = (i == selected_index_);
                
                auto srv_row = hbox({
                    text(srv.name) | (is_selected ? bold : nothing) | color(is_selected ? theme::Text : theme::TextDim),
                    filler(),
                    text(srv.ip_address) | color(theme::TextDim),
                    text("  (" + std::to_string(srv.latency_ms) + " ms) ") | color(theme::latency_color(srv.latency_ms)),
                    text(srv.is_paired ? "[Paired]" : "[New]") | color(srv.is_paired ? theme::Success : theme::Warning)
                });

                if (is_selected) {
                    srv_row = srv_row | bgcolor(theme::Border);
                }

                elements.push_back(srv_row);
            }
        }
        
        elements.push_back(separator());
        elements.push_back(
            text("Use ARROWS to select, ENTER to connect, F2 for Settings") | color(theme::TextDim) | center
        );

        return vbox(elements) | theme::window_box() | size(WIDTH, GREATER_THAN, 60);
    });

    auto event_handler = CatchEvent(menu_renderer, [this](Event e) {
        if (servers_.empty()) return false;

        if (e == Event::ArrowUp && selected_index_ > 0) {
            selected_index_--;
            return true;
        }
        if (e == Event::ArrowDown && selected_index_ < static_cast<int>(servers_.size() - 1)) {
            selected_index_++;
            return true;
        }
        if (e == Event::Return) {
            if (connect_callback_) {
                connect_callback_(servers_[selected_index_]);
            }
            return true;
        }
        return false;
    });

    return event_handler;
}

} // namespace ud
