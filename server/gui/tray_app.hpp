#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>

namespace ud {

class TrayApp {
public:
    TrayApp();
    ~TrayApp();

    bool init(HINSTANCE hinstance);
    void run();

    // Triggered by the server core to show pairing requests
    void show_pairing_dialog(const std::string& device_name, const std::string& fingerprint);
    
    // Show a balloon tooltip notification
    void show_notification(const std::string& title, const std::string& message);

private:
    HINSTANCE hinstance_{nullptr};
    HWND hwnd_{nullptr};
    NOTIFYICONDATAA nid_{};

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void show_context_menu(POINT pt);
};

} // namespace ud
