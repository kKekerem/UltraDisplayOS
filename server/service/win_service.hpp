#pragma once


#include <windows.h>
#include <string>

namespace ud {

class WinService {
public:
    static bool install(const std::wstring& service_name, const std::wstring& display_name);
    static bool uninstall(const std::wstring& service_name);
    
    // Starts the Service Control Dispatcher. Blocks until service stops.
    static bool run(const std::wstring& service_name);

private:
    static std::wstring service_name_;
    static SERVICE_STATUS_HANDLE status_handle_;
    static SERVICE_STATUS status_;

    static void WINAPI service_main(DWORD argc, LPWSTR* argv);
    static void WINAPI service_ctrl_handler(DWORD ctrl_code);
    
    static void set_status(DWORD state, DWORD exit_code = NO_ERROR, DWORD wait_hint = 0);
};

} // namespace ud
