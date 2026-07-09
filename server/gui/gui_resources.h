#pragma once

// ---- Dialog / Window IDs ----
#define IDD_MAIN_DIALOG         100

// ---- Control IDs ----
#define IDC_TAB_CONTROL         1001

// Devices tab
#define IDC_DEVICE_LIST         1010
#define IDC_BTN_SCAN            1011
#define IDC_BTN_CONNECT         1012
#define IDC_BTN_DISCONNECT      1013

// Display Settings tab
#define IDC_COMBO_MONITOR       1020
#define IDC_COMBO_RESOLUTION    1021
#define IDC_COMBO_FPS           1022
#define IDC_COMBO_CODEC         1023
#define IDC_COMBO_QUALITY       1024

// Network tab
#define IDC_SLIDER_BANDWIDTH        1030
#define IDC_STATIC_BANDWIDTH_VALUE  1031
#define IDC_STATIC_LATENCY          1032
#define IDC_STATIC_FPS_STAT         1033
#define IDC_STATIC_BITRATE          1034
#define IDC_STATIC_PACKETLOSS       1035

// ---- Tray Icon ----
#define IDI_TRAY_ICON           2000
#define IDM_TRAY_MENU           2001
#define IDM_OPEN                2010
#define IDM_EXIT                2011

// ---- Custom Messages ----
#define WM_TRAY_ICON            (WM_USER + 1)
