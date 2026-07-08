#include "server/gui/tray_app.hpp"

#include <cstring>

namespace {
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kTrayIconId = 1;
constexpr UINT kExitCommand = 1001;
constexpr const char* kWindowClassName = "UltraDisplayTrayWindow";

void copy_truncated(char* dest, size_t dest_size, const std::string& value) {
    if (dest_size == 0) {
        return;
    }

    strncpy_s(dest, dest_size, value.c_str(), _TRUNCATE);
}
} // namespace

namespace ud {

TrayApp::TrayApp() = default;

TrayApp::~TrayApp() {
    if (nid_.cbSize != 0) {
        Shell_NotifyIconA(NIM_DELETE, &nid_);
    }
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

bool TrayApp::init(HINSTANCE hinstance) {
    hinstance_ = hinstance;

    WNDCLASSA wc{};
    wc.lpfnWndProc = TrayApp::window_proc;
    wc.hInstance = hinstance_;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassA(&wc);

    hwnd_ = CreateWindowExA(
        0,
        kWindowClassName,
        "UltraDisplay Server",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        nullptr,
        nullptr,
        hinstance_,
        this);

    if (!hwnd_) {
        return false;
    }

    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd_;
    nid_.uID = kTrayIconId;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = kTrayMessage;
    nid_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    copy_truncated(nid_.szTip, sizeof(nid_.szTip), "UltraDisplay Server");

    if (!Shell_NotifyIconA(NIM_ADD, &nid_)) {
        return false;
    }

    nid_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconA(NIM_SETVERSION, &nid_);
    return true;
}

void TrayApp::run() {
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void TrayApp::show_pairing_dialog(const std::string& device_name, const std::string& fingerprint) {
    const std::string message =
        "Pairing request from " + device_name + "\n\nFingerprint:\n" + fingerprint;
    MessageBoxA(hwnd_, message.c_str(), "UltraDisplay Pairing", MB_OK | MB_ICONINFORMATION);
}

void TrayApp::show_notification(const std::string& title, const std::string& message) {
    nid_.uFlags = NIF_INFO;
    copy_truncated(nid_.szInfoTitle, sizeof(nid_.szInfoTitle), title);
    copy_truncated(nid_.szInfo, sizeof(nid_.szInfo), message);
    nid_.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconA(NIM_MODIFY, &nid_);
}

LRESULT CALLBACK TrayApp::window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }

    auto* app = reinterpret_cast<TrayApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (uMsg == kTrayMessage && app) {
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
            POINT pt{};
            GetCursorPos(&pt);
            app->show_context_menu(pt);
            return 0;
        }
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            app->show_notification("UltraDisplay Server", "Server is running.");
            return 0;
        }
    }

    switch (uMsg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == kExitCommand) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void TrayApp::show_context_menu(POINT pt) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    AppendMenuA(menu, MF_STRING, kExitCommand, "Exit");
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

} // namespace ud
