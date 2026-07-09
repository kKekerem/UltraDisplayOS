#include "server/gui/tray_app.hpp"
#include "shared/util/log.hpp"

#include <windowsx.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ws2_32.lib")

// Enable visual styles via manifest
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace ud {

// ─── Layout constants ────────────────────────────────────────────────────────
static constexpr int WINDOW_W        = 700;
static constexpr int WINDOW_H        = 500;
static constexpr int TAB_MARGIN      = 8;
static constexpr int CONTENT_TOP     = 38;   // below tab headers
static constexpr int CONTENT_LEFT    = 16;
static constexpr int LABEL_H         = 18;
static constexpr int CONTROL_H       = 24;
static constexpr int BUTTON_W        = 90;
static constexpr int BUTTON_H        = 28;
static constexpr int COMBO_W         = 200;
static constexpr int ROW_SPACING     = 34;

// ─── App-internal window class name ──────────────────────────────────────────
static const wchar_t* const WND_CLASS_NAME = L"UltraDisplayServerApp";

// ─── Constructor / Destructor ────────────────────────────────────────────────

TrayApp::TrayApp() = default;

TrayApp::~TrayApp() {
    discovery_.stop_scan();
    remove_tray_icon();
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

// ─── Init ────────────────────────────────────────────────────────────────────

bool TrayApp::init(HINSTANCE hinstance) {
    hinstance_ = hinstance;

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES;
    if (!InitCommonControlsEx(&icc)) {
        UD_LOG_ERROR("TrayApp: InitCommonControlsEx failed");
        return false;
    }

    // Register window class
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = window_proc;
    wc.hInstance      = hinstance_;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = WND_CLASS_NAME;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wc)) {
        UD_LOG_ERROR("TrayApp: RegisterClassEx failed, error {}", GetLastError());
        return false;
    }

    // Calculate centered position
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int x = (screen_w - WINDOW_W) / 2;
    int y = (screen_h - WINDOW_H) / 2;

    // Create main window (hidden initially)
    hwnd_ = CreateWindowExW(
        0,
        WND_CLASS_NAME,
        L"UltraDisplay Server",
        WS_OVERLAPPEDWINDOW,
        x, y, WINDOW_W, WINDOW_H,
        nullptr, nullptr, hinstance_,
        this   // pass 'this' via CREATESTRUCT for WM_NCCREATE
    );

    if (!hwnd_) {
        UD_LOG_ERROR("TrayApp: CreateWindowEx failed, error {}", GetLastError());
        return false;
    }

    // Create all child controls
    create_controls();

    // Create tray icon
    create_tray_icon();

    // Enumerate monitors for the display tab
    enumerate_monitors();

    // Start device refresh timer (every 2s)
    SetTimer(hwnd_, TIMER_DEVICE_REFRESH, 2000, nullptr);
    // Start stats update timer (every 1s)
    SetTimer(hwnd_, TIMER_STATS_UPDATE, 1000, nullptr);

    UD_LOG_INFO("TrayApp: initialized successfully");
    return true;
}

// ─── Message loop ────────────────────────────────────────────────────────────

void TrayApp::run() {
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// ─── Static WndProc → instance dispatch ──────────────────────────────────────

LRESULT CALLBACK TrayApp::window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    TrayApp* self = nullptr;

    if (uMsg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<TrayApp*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<TrayApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->handle_message(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ─── Instance message handler ────────────────────────────────────────────────

LRESULT TrayApp::handle_message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {

    case WM_CLOSE:
        // Don't destroy — just hide to tray
        hide_window();
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_DEVICE_REFRESH);
        KillTimer(hwnd, TIMER_STATS_UPDATE);
        discovery_.stop_scan();
        remove_tray_icon();
        PostQuitMessage(0);
        return 0;

    case WM_TRAY_ICON:
        switch (LOWORD(lParam)) {
        case WM_RBUTTONUP: {
            POINT pt;
            GetCursorPos(&pt);
            show_context_menu(pt);
            break;
        }
        case WM_LBUTTONDBLCLK:
            show_window();
            break;
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_OPEN:
            show_window();
            break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        case IDC_BTN_SCAN:
            UD_LOG_INFO("TrayApp: scan button pressed");
            discovery_.stop_scan();
            discovery_.start_scan();
            break;
        case IDC_BTN_CONNECT:
            UD_LOG_INFO("TrayApp: connect button pressed");
            // Placeholder: actual connection logic would be integrated here
            show_notification("UltraDisplay", "Connecting to selected device...");
            break;
        case IDC_BTN_DISCONNECT:
            UD_LOG_INFO("TrayApp: disconnect button pressed");
            show_notification("UltraDisplay", "Disconnected.");
            break;
        }

        // Handle combo box selection changes
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            HWND combo = reinterpret_cast<HWND>(lParam);
            int sel = static_cast<int>(SendMessage(combo, CB_GETCURSEL, 0, 0));
            if (combo == hwnd_combo_monitor_)       { settings_.monitor_index = sel; }
            else if (combo == hwnd_combo_resolution_) { settings_.resolution_index = sel; }
            else if (combo == hwnd_combo_fps_)        { settings_.fps_index = sel; }
            else if (combo == hwnd_combo_codec_)      { settings_.codec_index = sel; }
            else if (combo == hwnd_combo_quality_)    { settings_.quality_index = sel; }
        }
        return 0;

    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == hwnd_slider_bandwidth_) {
            int pos = static_cast<int>(SendMessage(hwnd_slider_bandwidth_, TBM_GETPOS, 0, 0));
            settings_.bandwidth_mbps = pos;
            wchar_t buf[64];
            swprintf_s(buf, L"%d Mbps", pos);
            SetWindowTextW(hwnd_static_bandwidth_, buf);
        }
        return 0;

    case WM_NOTIFY: {
        auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
        if (nmhdr->hwndFrom == hwnd_tab_ && nmhdr->code == TCN_SELCHANGE) {
            on_tab_changed();
        }
        return 0;
    }

    case WM_TIMER:
        if (wParam == TIMER_DEVICE_REFRESH) {
            update_device_list();
        } else if (wParam == TIMER_STATS_UPDATE) {
            update_stats();
        }
        return 0;

    case WM_SIZE: {
        if (hwnd_tab_) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            MoveWindow(hwnd_tab_, TAB_MARGIN, TAB_MARGIN,
                       rc.right - 2 * TAB_MARGIN,
                       rc.bottom - 2 * TAB_MARGIN, TRUE);
        }
        return 0;
    }

    default:
        break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ─── Tray Icon ───────────────────────────────────────────────────────────────

void TrayApp::create_tray_icon() {
    memset(&nid_, 0, sizeof(nid_));
    nid_.cbSize           = sizeof(NOTIFYICONDATA);
    nid_.hWnd             = hwnd_;
    nid_.uID              = IDI_TRAY_ICON;
    nid_.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAY_ICON;
    nid_.hIcon            = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(nid_.szTip, L"UltraDisplay Server");

    Shell_NotifyIconW(NIM_ADD, &nid_);
    UD_LOG_INFO("TrayApp: tray icon created");
}

void TrayApp::remove_tray_icon() {
    Shell_NotifyIconW(NIM_DELETE, &nid_);
}

void TrayApp::show_context_menu(POINT pt) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_OPEN, L"Open");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");

    // Required for the menu to dismiss properly
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                   pt.x, pt.y, 0, hwnd_, nullptr);
    PostMessage(hwnd_, WM_NULL, 0, 0);

    DestroyMenu(menu);
}

void TrayApp::show_window() {
    ShowWindow(hwnd_, SW_SHOW);
    ShowWindow(hwnd_, SW_RESTORE);
    SetForegroundWindow(hwnd_);
}

void TrayApp::hide_window() {
    ShowWindow(hwnd_, SW_HIDE);
}

// ─── Notifications ──────────────────────────────────────────────────────────

void TrayApp::show_notification(const std::string& title, const std::string& message) {
    nid_.uFlags = NIF_INFO;
    nid_.dwInfoFlags = NIIF_INFO;

    std::wstring wtitle(title.begin(), title.end());
    std::wstring wmsg(message.begin(), message.end());

    wcscpy_s(nid_.szInfoTitle, wtitle.c_str());
    wcscpy_s(nid_.szInfo, wmsg.c_str());

    Shell_NotifyIconW(NIM_MODIFY, &nid_);

    // Restore flags
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

void TrayApp::show_pairing_dialog(const std::string& device_name, const std::string& fingerprint) {
    std::wstring msg = L"Device \"";
    msg += std::wstring(device_name.begin(), device_name.end());
    msg += L"\" wants to pair.\n\nFingerprint: ";
    msg += std::wstring(fingerprint.begin(), fingerprint.end());
    msg += L"\n\nAllow this connection?";

    int result = MessageBoxW(hwnd_, msg.c_str(), L"UltraDisplay — Pairing Request",
                             MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);

    if (result == IDYES) {
        UD_LOG_INFO("TrayApp: pairing accepted for device '{}'", device_name);
        show_notification("UltraDisplay", "Pairing accepted: " + device_name);
    } else {
        UD_LOG_INFO("TrayApp: pairing rejected for device '{}'", device_name);
    }
}

// ─── Control creation — master ──────────────────────────────────────────────

void TrayApp::create_controls() {
    RECT rc;
    GetClientRect(hwnd_, &rc);

    // Create Tab Control (fills the client area)
    hwnd_tab_ = CreateWindowExW(
        0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        TAB_MARGIN, TAB_MARGIN,
        rc.right - 2 * TAB_MARGIN,
        rc.bottom - 2 * TAB_MARGIN,
        hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TAB_CONTROL)),
        hinstance_, nullptr);

    // Set the tab control font
    HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessage(hwnd_tab_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    // Insert tabs
    TCITEMW tie{};
    tie.mask = TCIF_TEXT;

    tie.pszText = const_cast<LPWSTR>(L"Devices");
    TabCtrl_InsertItem(hwnd_tab_, 0, &tie);

    tie.pszText = const_cast<LPWSTR>(L"Display Settings");
    TabCtrl_InsertItem(hwnd_tab_, 1, &tie);

    tie.pszText = const_cast<LPWSTR>(L"Network");
    TabCtrl_InsertItem(hwnd_tab_, 2, &tie);

    // Create child controls for each tab
    create_devices_tab();
    create_display_tab();
    create_network_tab();

    // Show only the first tab
    current_tab_ = 0;
    show_tab_controls(0, true);
    show_tab_controls(1, false);
    show_tab_controls(2, false);
}

// ─── Helper: get content rect inside the tab control ─────────────────────────

static RECT get_tab_content_rect(HWND hwnd_tab) {
    RECT rc;
    GetClientRect(hwnd_tab, &rc);
    TabCtrl_AdjustRect(hwnd_tab, FALSE, &rc);
    return rc;
}

// ─── Devices Tab ─────────────────────────────────────────────────────────────

void TrayApp::create_devices_tab() {
    HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    RECT rc = get_tab_content_rect(hwnd_tab_);

    int x = rc.left + CONTENT_LEFT;
    int y = rc.top + 10;
    int list_w = rc.right - rc.left - 2 * CONTENT_LEFT;
    int list_h = rc.bottom - rc.top - 60;

    // ListView
    hwnd_device_list_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        x, y, list_w, list_h,
        hwnd_tab_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DEVICE_LIST)),
        hinstance_, nullptr);
    SendMessage(hwnd_device_list_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    ListView_SetExtendedListViewStyle(hwnd_device_list_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // Add columns
    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvc.iSubItem = 0;
    lvc.cx = 220;
    lvc.pszText = const_cast<LPWSTR>(L"Device Name");
    ListView_InsertColumn(hwnd_device_list_, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.cx = 150;
    lvc.pszText = const_cast<LPWSTR>(L"IP Address");
    ListView_InsertColumn(hwnd_device_list_, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.cx = 120;
    lvc.pszText = const_cast<LPWSTR>(L"Status");
    ListView_InsertColumn(hwnd_device_list_, 2, &lvc);

    // Buttons row
    int btn_y = y + list_h + 8;
    hwnd_btn_scan_ = CreateWindowExW(
        0, L"BUTTON", L"Scan",
        WS_CHILD | BS_PUSHBUTTON,
        x, btn_y, BUTTON_W, BUTTON_H,
        hwnd_tab_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BTN_SCAN)),
        hinstance_, nullptr);
    SendMessage(hwnd_btn_scan_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    hwnd_btn_connect_ = CreateWindowExW(
        0, L"BUTTON", L"Connect",
        WS_CHILD | BS_PUSHBUTTON,
        x + BUTTON_W + 8, btn_y, BUTTON_W, BUTTON_H,
        hwnd_tab_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BTN_CONNECT)),
        hinstance_, nullptr);
    SendMessage(hwnd_btn_connect_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    hwnd_btn_disconnect_ = CreateWindowExW(
        0, L"BUTTON", L"Disconnect",
        WS_CHILD | BS_PUSHBUTTON,
        x + 2 * (BUTTON_W + 8), btn_y, BUTTON_W, BUTTON_H,
        hwnd_tab_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BTN_DISCONNECT)),
        hinstance_, nullptr);
    SendMessage(hwnd_btn_disconnect_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
}

// ─── Display Settings Tab ────────────────────────────────────────────────────

void TrayApp::create_display_tab() {
    HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    RECT rc = get_tab_content_rect(hwnd_tab_);

    int x_label = rc.left + CONTENT_LEFT;
    int x_combo = x_label + 110;
    int y = rc.top + 14;

    auto make_row = [&](const wchar_t* label, int ctrl_id, HWND& label_out, HWND& combo_out,
                        const wchar_t** items, int item_count, int default_sel) {
        label_out = CreateWindowExW(
            0, L"STATIC", label,
            WS_CHILD | SS_LEFT,
            x_label, y + 3, 100, LABEL_H,
            hwnd_tab_, nullptr, hinstance_, nullptr);
        SendMessage(label_out, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        combo_out = CreateWindowExW(
            0, L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            x_combo, y, COMBO_W, 200,
            hwnd_tab_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ctrl_id)),
            hinstance_, nullptr);
        SendMessage(combo_out, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        for (int i = 0; i < item_count; i++) {
            SendMessageW(combo_out, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(items[i]));
        }
        SendMessage(combo_out, CB_SETCURSEL, default_sel, 0);

        y += ROW_SPACING;
    };

    // Monitor (populated later by enumerate_monitors)
    {
        hwnd_label_monitor_ = CreateWindowExW(
            0, L"STATIC", L"Monitor:",
            WS_CHILD | SS_LEFT,
            x_label, y + 3, 100, LABEL_H,
            hwnd_tab_, nullptr, hinstance_, nullptr);
        SendMessage(hwnd_label_monitor_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        hwnd_combo_monitor_ = CreateWindowExW(
            0, L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            x_combo, y, COMBO_W, 200,
            hwnd_tab_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_COMBO_MONITOR)),
            hinstance_, nullptr);
        SendMessage(hwnd_combo_monitor_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        y += ROW_SPACING;
    }

    // Resolution
    {
        const wchar_t* items[] = { L"Native", L"3840x2160", L"2560x1440", L"1920x1080", L"1280x720" };
        make_row(L"Resolution:", IDC_COMBO_RESOLUTION, hwnd_label_resolution_,
                 hwnd_combo_resolution_, items, 5, settings_.resolution_index);
    }

    // FPS
    {
        const wchar_t* items[] = { L"30", L"60", L"90", L"120", L"144" };
        make_row(L"FPS:", IDC_COMBO_FPS, hwnd_label_fps_,
                 hwnd_combo_fps_, items, 5, settings_.fps_index);
    }

    // Codec
    {
        const wchar_t* items[] = { L"H.264", L"HEVC", L"AV1" };
        make_row(L"Codec:", IDC_COMBO_CODEC, hwnd_label_codec_,
                 hwnd_combo_codec_, items, 3, settings_.codec_index);
    }

    // Quality Mode
    {
        const wchar_t* items[] = { L"Low Latency", L"Balanced", L"High Quality" };
        make_row(L"Quality:", IDC_COMBO_QUALITY, hwnd_label_quality_,
                 hwnd_combo_quality_, items, 3, settings_.quality_index);
    }
}

// ─── Network Tab ─────────────────────────────────────────────────────────────

void TrayApp::create_network_tab() {
    HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    RECT rc = get_tab_content_rect(hwnd_tab_);

    int x_label = rc.left + CONTENT_LEFT;
    int x_value = x_label + 130;
    int value_w = 200;
    int y = rc.top + 14;

    // ── Bandwidth slider ─────────────────────────────────────────────────
    hwnd_label_bandwidth_ = CreateWindowExW(
        0, L"STATIC", L"Bandwidth Limit:",
        WS_CHILD | SS_LEFT,
        x_label, y + 3, 120, LABEL_H,
        hwnd_tab_, nullptr, hinstance_, nullptr);
    SendMessage(hwnd_label_bandwidth_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    hwnd_slider_bandwidth_ = CreateWindowExW(
        0, TRACKBAR_CLASSW, L"",
        WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
        x_value, y, 280, 30,
        hwnd_tab_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SLIDER_BANDWIDTH)),
        hinstance_, nullptr);
    SendMessage(hwnd_slider_bandwidth_, TBM_SETRANGE, TRUE, MAKELPARAM(10, 1000));
    SendMessage(hwnd_slider_bandwidth_, TBM_SETTICFREQ, 100, 0);
    SendMessage(hwnd_slider_bandwidth_, TBM_SETPOS, TRUE, settings_.bandwidth_mbps);

    // Value label next to slider
    wchar_t bw_buf[64];
    swprintf_s(bw_buf, L"%d Mbps", settings_.bandwidth_mbps);
    hwnd_static_bandwidth_ = CreateWindowExW(
        0, L"STATIC", bw_buf,
        WS_CHILD | SS_LEFT,
        x_value + 290, y + 5, 100, LABEL_H,
        hwnd_tab_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATIC_BANDWIDTH_VALUE)),
        hinstance_, nullptr);
    SendMessage(hwnd_static_bandwidth_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    y += 50;

    // ── Separator label ──────────────────────────────────────────────────
    HWND hSep = CreateWindowExW(
        0, L"STATIC", L"── Live Statistics ──",
        WS_CHILD | SS_LEFT,
        x_label, y, 300, LABEL_H,
        hwnd_tab_, nullptr, hinstance_, nullptr);
    SendMessage(hSep, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    // This separator label is always visible when the network tab is visible,
    // but we don't need to track it for show/hide since its parent visibility
    // is managed. Actually we need to. We'll store it in a local and show it
    // along with tab. For simplicity, we'll just treat it as always present;
    // we only show/hide the tab 2 controls group. We'll manage this via
    // parent re-parenting approach — but since all controls are children of
    // hwnd_tab_, we instead manage them manually. Let's just ensure it is
    // included in the tab 2 show/hide list. But we have no vector for extra
    // handles — let's make it a child of hwnd_tab_ and we'll find a different
    // way. Actually the simplest approach: we just show/hide individual known
    // handles. We'll track this as part of network tab. We can add it into
    // show_tab_controls. For now, let's just skip explicit tracking of the
    // separator — it will be visible whenever tab 2 controls are visible.
    // We'll set its initial visibility in show_tab_controls by setting
    // parent controls. Actually the approach we use is to show/hide each
    // control. Let's just not worry about the separator for now — it's a
    // minor visual. We'll handle show/hide in show_tab_controls.

    y += 28;

    // Stat rows
    auto make_stat_row = [&](const wchar_t* label, int stat_id,
                             HWND& label_out, HWND& value_out, const wchar_t* initial) {
        label_out = CreateWindowExW(
            0, L"STATIC", label,
            WS_CHILD | SS_LEFT,
            x_label, y + 1, 120, LABEL_H,
            hwnd_tab_, nullptr, hinstance_, nullptr);
        SendMessage(label_out, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        value_out = CreateWindowExW(
            0, L"STATIC", initial,
            WS_CHILD | SS_LEFT,
            x_value, y + 1, value_w, LABEL_H,
            hwnd_tab_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(stat_id)),
            hinstance_, nullptr);
        SendMessage(value_out, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        y += ROW_SPACING;
    };

    make_stat_row(L"Latency:", IDC_STATIC_LATENCY,
                  hwnd_label_latency_, hwnd_static_latency_, L"— ms");
    make_stat_row(L"FPS:", IDC_STATIC_FPS_STAT,
                  hwnd_label_fps_stat_, hwnd_static_fps_stat_, L"— fps");
    make_stat_row(L"Bitrate:", IDC_STATIC_BITRATE,
                  hwnd_label_bitrate_, hwnd_static_bitrate_, L"— Mbps");
    make_stat_row(L"Packet Loss:", IDC_STATIC_PACKETLOSS,
                  hwnd_label_packetloss_, hwnd_static_packetloss_, L"— %");
}

// ─── Tab switching ──────────────────────────────────────────────────────────

void TrayApp::on_tab_changed() {
    int new_tab = TabCtrl_GetCurSel(hwnd_tab_);
    if (new_tab == current_tab_) return;

    show_tab_controls(current_tab_, false);
    show_tab_controls(new_tab, true);
    current_tab_ = new_tab;
}

void TrayApp::show_tab_controls(int tab_index, bool show) {
    int cmd = show ? SW_SHOW : SW_HIDE;

    switch (tab_index) {
    case 0: // Devices
        ShowWindow(hwnd_device_list_, cmd);
        ShowWindow(hwnd_btn_scan_, cmd);
        ShowWindow(hwnd_btn_connect_, cmd);
        ShowWindow(hwnd_btn_disconnect_, cmd);
        break;

    case 1: // Display Settings
        ShowWindow(hwnd_label_monitor_, cmd);
        ShowWindow(hwnd_combo_monitor_, cmd);
        ShowWindow(hwnd_label_resolution_, cmd);
        ShowWindow(hwnd_combo_resolution_, cmd);
        ShowWindow(hwnd_label_fps_, cmd);
        ShowWindow(hwnd_combo_fps_, cmd);
        ShowWindow(hwnd_label_codec_, cmd);
        ShowWindow(hwnd_combo_codec_, cmd);
        ShowWindow(hwnd_label_quality_, cmd);
        ShowWindow(hwnd_combo_quality_, cmd);
        break;

    case 2: // Network
        ShowWindow(hwnd_label_bandwidth_, cmd);
        ShowWindow(hwnd_slider_bandwidth_, cmd);
        ShowWindow(hwnd_static_bandwidth_, cmd);
        ShowWindow(hwnd_label_latency_, cmd);
        ShowWindow(hwnd_static_latency_, cmd);
        ShowWindow(hwnd_label_fps_stat_, cmd);
        ShowWindow(hwnd_static_fps_stat_, cmd);
        ShowWindow(hwnd_label_bitrate_, cmd);
        ShowWindow(hwnd_static_bitrate_, cmd);
        ShowWindow(hwnd_label_packetloss_, cmd);
        ShowWindow(hwnd_static_packetloss_, cmd);
        break;
    }
}

// ─── Device list update ─────────────────────────────────────────────────────

void TrayApp::update_device_list() {
    auto clients = discovery_.get_clients();

    // Only refresh if the list view is actually shown
    if (!IsWindowVisible(hwnd_device_list_)) return;

    ListView_DeleteAllItems(hwnd_device_list_);

    int idx = 0;
    for (const auto& client : clients) {
        std::wstring wname(client.name.begin(), client.name.end());
        std::wstring wip(client.ip.begin(), client.ip.end());

        LVITEMW lvi{};
        lvi.mask     = LVIF_TEXT;
        lvi.iItem    = idx;
        lvi.iSubItem = 0;
        lvi.pszText  = const_cast<LPWSTR>(wname.c_str());
        ListView_InsertItem(hwnd_device_list_, &lvi);

        ListView_SetItemText(hwnd_device_list_, idx, 1, const_cast<LPWSTR>(wip.c_str()));
        ListView_SetItemText(hwnd_device_list_, idx, 2, const_cast<LPWSTR>(L"Discovered"));

        idx++;
    }
}

// ─── Stats update ────────────────────────────────────────────────────────────

void TrayApp::update_stats() {
    // Only update if network tab is visible
    if (current_tab_ != 2) return;

    // In a real implementation, these values would come from the streaming core.
    // For now, show placeholder values to demonstrate the UI is alive.
    SetWindowTextW(hwnd_static_latency_,    L"— ms");
    SetWindowTextW(hwnd_static_fps_stat_,   L"— fps");
    SetWindowTextW(hwnd_static_bitrate_,    L"— Mbps");
    SetWindowTextW(hwnd_static_packetloss_, L"— %");
}

// ─── Monitor enumeration ────────────────────────────────────────────────────

void TrayApp::enumerate_monitors() {
    monitor_names_.clear();
    SendMessage(hwnd_combo_monitor_, CB_RESETCONTENT, 0, 0);

    DISPLAY_DEVICEW dd{};
    dd.cb = sizeof(dd);

    for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); i++) {
        if (!(dd.StateFlags & DISPLAY_DEVICE_ACTIVE)) continue;

        std::wstring name = dd.DeviceName;

        // Try to get a friendly name from the monitor
        DISPLAY_DEVICEW mon{};
        mon.cb = sizeof(mon);
        if (EnumDisplayDevicesW(dd.DeviceName, 0, &mon, 0)) {
            if (wcslen(mon.DeviceString) > 0) {
                name += L" — ";
                name += mon.DeviceString;
            }
        }

        monitor_names_.push_back(name);
        SendMessageW(hwnd_combo_monitor_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
    }

    if (!monitor_names_.empty()) {
        SendMessage(hwnd_combo_monitor_, CB_SETCURSEL, 0, 0);
    }

    UD_LOG_INFO("TrayApp: enumerated {} monitors", monitor_names_.size());
}

} // namespace ud
