#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <atomic>
#include <string>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// ============ 全局状态 ============
extern std::atomic<bool> running;
extern std::atomic<bool> isWindowVisible;
extern std::atomic<float> scaleFactor;
extern std::atomic<float> opacityFactor;
extern std::atomic<bool> grayscaleEnabled;
extern std::atomic<bool> removeWhiteBg;
extern std::atomic<bool> reloadImage;
extern std::atomic<bool> autoLoadLatest;

extern std::wstring currentImagePath;
extern std::wstring imageDirectory;

extern std::atomic<int> windowOffsetX;
extern std::atomic<int> windowOffsetY;

extern HWND g_hwndMain;
extern HWND g_hwndSettings;
extern HINSTANCE g_hInstance;
extern NOTIFYICONDATAW g_nid;

// ============ 常量定义 ============
#define WM_TRAYICON      (WM_USER + 1)
#define IDM_SHOW_HIDE    1001
#define IDM_SETTINGS     1002
#define IDM_RELOAD       1003
#define IDM_EXIT         1004

#define IDC_SLIDER_OPACITY    2001
#define IDC_SLIDER_SCALE      2002
#define IDC_CHECK_GRAYSCALE   2003
#define IDC_CHECK_REMOVEWHITE 2004
#define IDC_CHECK_AUTOLOAD    2005
#define IDC_EDIT_PATH         2006
#define IDC_BTN_BROWSE        2007
#define IDC_LABEL_OPACITY     2008
#define IDC_LABEL_SCALE       2009
#define IDC_BTN_APPLY         2010
