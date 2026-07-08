#include "server/service/win_service.hpp"

namespace ud {

std::wstring WinService::service_name_;
SERVICE_STATUS_HANDLE WinService::status_handle_ = nullptr;
SERVICE_STATUS WinService::status_{};

bool WinService::install(const std::wstring& service_name, const std::wstring& display_name) {
    wchar_t exe_path[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) == 0) {
        return false;
    }

    const std::wstring command = L"\"" + std::wstring(exe_path) + L"\" --service";

    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        return false;
    }

    SC_HANDLE service = CreateServiceW(
        scm,
        service_name.c_str(),
        display_name.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        command.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return true;
}

bool WinService::uninstall(const std::wstring& service_name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        return false;
    }

    SC_HANDLE service = OpenServiceW(scm, service_name.c_str(), SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS status{};
    ControlService(service, SERVICE_CONTROL_STOP, &status);
    const bool deleted = DeleteService(service) != 0;

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return deleted;
}

bool WinService::run(const std::wstring& service_name) {
    service_name_ = service_name;

    SERVICE_TABLE_ENTRYW table[] = {
        {service_name_.data(), WinService::service_main},
        {nullptr, nullptr},
    };

    return StartServiceCtrlDispatcherW(table) != 0;
}

void WINAPI WinService::service_main(DWORD argc, LPWSTR* argv) {
    (void)argc;
    (void)argv;

    status_handle_ = RegisterServiceCtrlHandlerW(service_name_.c_str(), WinService::service_ctrl_handler);
    if (!status_handle_) {
        return;
    }

    status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status_.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    set_status(SERVICE_START_PENDING, NO_ERROR, 1000);
    set_status(SERVICE_RUNNING);

    while (status_.dwCurrentState == SERVICE_RUNNING) {
        Sleep(1000);
    }
}

void WINAPI WinService::service_ctrl_handler(DWORD ctrl_code) {
    switch (ctrl_code) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        set_status(SERVICE_STOP_PENDING, NO_ERROR, 1000);
        set_status(SERVICE_STOPPED);
        break;
    default:
        if (status_handle_) {
            SetServiceStatus(status_handle_, &status_);
        }
        break;
    }
}

void WinService::set_status(DWORD state, DWORD exit_code, DWORD wait_hint) {
    status_.dwCurrentState = state;
    status_.dwWin32ExitCode = exit_code;
    status_.dwWaitHint = wait_hint;
    status_.dwControlsAccepted =
        (state == SERVICE_START_PENDING || state == SERVICE_STOPPED)
            ? 0
            : SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    if (status_handle_) {
        SetServiceStatus(status_handle_, &status_);
    }
}

} // namespace ud
