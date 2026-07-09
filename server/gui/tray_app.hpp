#pragma once

// winsock2.h MUST come before windows.h to avoid redefinition errors
#include <winsock2.h>


#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>

#include "server/gui/gui_resources.h"
#include "server/gui/discovery.hpp"

#include <string>
#include <vector>

namespace ud {

// ─── Timer IDs ───────────────────────────────────────────────────────────────
static constexpr UINT_PTR TIMER_DEVICE_REFRESH = 1;
static constexpr UINT_PTR TIMER_STATS_UPDATE   = 2;

// ─── Display settings ────────────────────────────────────────────────────────
struct DisplaySettings {
    int  monitor_index{0};
    int  resolution_index{0};   // 0=Native, 1=3840x2160, …
    int  fps_index{1};          // default 60 fps
    int  codec_index{0};        // 0=H.264
    int  quality_index{1};      // 0=Low Latency, 1=Balanced, 2=High Quality
    int  bandwidth_mbps{200};
};

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
    // ── Window / instance ────────────────────────────────────────────────
    HINSTANCE hinstance_{nullptr};
    HWND      hwnd_{nullptr};
    NOTIFYICONDATA nid_{};

    // ── Tab control ──────────────────────────────────────────────────────
    HWND hwnd_tab_{nullptr};

    // ── Devices tab controls ─────────────────────────────────────────────
    HWND hwnd_device_list_{nullptr};
    HWND hwnd_btn_scan_{nullptr};
    HWND hwnd_btn_connect_{nullptr};
    HWND hwnd_btn_disconnect_{nullptr};

    // ── Display settings tab controls ────────────────────────────────────
    HWND hwnd_combo_monitor_{nullptr};
    HWND hwnd_combo_resolution_{nullptr};
    HWND hwnd_combo_fps_{nullptr};
    HWND hwnd_combo_codec_{nullptr};
    HWND hwnd_combo_quality_{nullptr};

    // ── Labels for display tab ───────────────────────────────────────────
    HWND hwnd_label_monitor_{nullptr};
    HWND hwnd_label_resolution_{nullptr};
    HWND hwnd_label_fps_{nullptr};
    HWND hwnd_label_codec_{nullptr};
    HWND hwnd_label_quality_{nullptr};

    // ── Network tab controls ─────────────────────────────────────────────
    HWND hwnd_slider_bandwidth_{nullptr};
    HWND hwnd_static_bandwidth_{nullptr};
    HWND hwnd_static_latency_{nullptr};
    HWND hwnd_static_fps_stat_{nullptr};
    HWND hwnd_static_bitrate_{nullptr};
    HWND hwnd_static_packetloss_{nullptr};

    // ── Labels for network tab ───────────────────────────────────────────
    HWND hwnd_label_bandwidth_{nullptr};
    HWND hwnd_label_latency_{nullptr};
    HWND hwnd_label_fps_stat_{nullptr};
    HWND hwnd_label_bitrate_{nullptr};
    HWND hwnd_label_packetloss_{nullptr};

    // ── Discovery ────────────────────────────────────────────────────────
    Discovery discovery_;

    // ── Settings ─────────────────────────────────────────────────────────
    DisplaySettings settings_;

    // ── Monitor names cache ──────────────────────────────────────────────
    std::vector<std::wstring> monitor_names_;

    // ── Current tab index ────────────────────────────────────────────────
    int current_tab_{0};

    // ── Window proc ──────────────────────────────────────────────────────
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handle_message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // ── Tray ─────────────────────────────────────────────────────────────
    void create_tray_icon();
    void remove_tray_icon();
    void show_context_menu(POINT pt);
    void show_window();
    void hide_window();

    // ── Control creation ─────────────────────────────────────────────────
    void create_controls();
    void create_devices_tab();
    void create_display_tab();
    void create_network_tab();

    // ── Tab switching ────────────────────────────────────────────────────
    void on_tab_changed();
    void show_tab_controls(int tab_index, bool show);

    // ── Device list updates ──────────────────────────────────────────────
    void update_device_list();
    void update_stats();

    // ── Monitor enumeration ──────────────────────────────────────────────
    void enumerate_monitors();
};

} // namespace ud
