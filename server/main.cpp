#include "shared/util/log.hpp"
#include "shared/util/thread.hpp"
#include "server/gui/tray_app.hpp"
#include "server/service/win_service.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace ud;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)nCmdShow;
    bool is_service = false;
    std::string cmd_line(lpCmdLine);
    
    if (cmd_line.find("--install") != std::string::npos) {
        return WinService::install(L"UltraDisplaySvc", L"UltraDisplay Server Core") ? 0 : 1;
    }
    if (cmd_line.find("--uninstall") != std::string::npos) {
        return WinService::uninstall(L"UltraDisplaySvc") ? 0 : 1;
    }
    if (cmd_line.find("--service") != std::string::npos) {
        is_service = true;
    }

    init_logging(true /* is_server */, true /* enable_debug */);
    set_thread_name("UD_Main");
    set_thread_realtime(); // High priority for capture scheduling

    UD_LOG_INFO("main", "UltraDisplay Server starting...");

    if (is_service) {
        // Run as Windows Service (background core)
        if (!WinService::run(L"UltraDisplaySvc")) {
            UD_LOG_ERROR("main", "Failed to start Windows Service");
            return 1;
        }
    } else {
        // Run as interactive tray app (UI layer)
        TrayApp app;
        if (!app.init(hInstance)) {
            UD_LOG_ERROR("main", "Failed to initialize Tray App");
            return 1;
        }
        app.run();
    }

    UD_LOG_INFO("main", "UltraDisplay Server shutting down.");
    return 0;
}
