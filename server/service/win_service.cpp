#include "win_service.hpp"
#include "shared/util/log.hpp"
#include <iostream>

namespace ud {

std::wstring WinService::service_name_;
SERVICE_STATUS_HANDLE WinService::status_handle_ = nullptr;
SERVICE_STATUS WinService::status_ = {};

bool WinService::install(const std::wstring& service_name, const std::wstring& display_name) {
    SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) return false;

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring cmd = std::wstring(path) + L" --service";

    SC_HANDLE service = CreateServiceW(
        scm, service_name.c_str(), display_name.c_str(),
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        cmd.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr
    );

    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return true;
}

bool WinService::uninstall(const std::wstring& service_name) {
    SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;

    SC_HANDLE service = OpenServiceW(scm, service_name.c_str(), DELETE);
    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    bool success = DeleteService(service);
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return success;
}

bool WinService::run(const std::wstring& service_name) {
    service_name_ = service_name;
    SERVICE_TABLE_ENTRYW dispatch_table[] = {
        { const_cast<LPWSTR>(service_name_.c_str()), (LPSERVICE_MAIN_FUNCTIONW)service_main },
        { nullptr, nullptr }
    };
    return StartServiceCtrlDispatcherW(dispatch_table) != 0;
}

void WINAPI WinService::service_main(DWORD argc, LPWSTR* argv) {
    (void)argc;
    (void)argv;
    status_handle_ = RegisterServiceCtrlHandlerW(service_name_.c_str(), service_ctrl_handler);
    if (!status_handle_) return;

    status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status_.dwServiceSpecificExitCode = 0;
    
    set_status(SERVICE_START_PENDING);
    set_status(SERVICE_RUNNING);

    // Keep service alive (dummy loop for now)
    HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    WaitForSingleObject(event, INFINITE);
    CloseHandle(event);

    set_status(SERVICE_STOPPED);
}

void WINAPI WinService::service_ctrl_handler(DWORD ctrl_code) {
    switch (ctrl_code) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            set_status(SERVICE_STOP_PENDING);
            ExitProcess(0);
            break;
        default:
            break;
    }
}

void WinService::set_status(DWORD state, DWORD exit_code, DWORD wait_hint) {
    status_.dwCurrentState = state;
    status_.dwWin32ExitCode = exit_code;
    status_.dwWaitHint = wait_hint;

    if (state == SERVICE_START_PENDING)
        status_.dwControlsAccepted = 0;
    else
        status_.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    if (state == SERVICE_RUNNING || state == SERVICE_STOPPED)
        status_.dwCheckPoint = 0;
    else
        status_.dwCheckPoint = 1;

    SetServiceStatus(status_handle_, &status_);
}

} // namespace ud
