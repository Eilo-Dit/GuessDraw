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

// ============ 快捷键配置 ============
enum HotkeyAction {
    HK_EXIT = 0,
    HK_TOGGLE_VISIBLE,
    HK_RELOAD,
    HK_OPACITY_UP,
    HK_OPACITY_DOWN,
    HK_SCALE_UP,
    HK_SCALE_DOWN,
    HK_DRAG_MODIFIER,   // 拖动修饰键
    HK_PREV_IMAGE,      // 上一张图片
    HK_NEXT_IMAGE,      // 下一张图片
    HK_COUNT
};

struct HotkeyBinding {
    int vkey;       // 主键 VK 码
    bool ctrl;      // 是否需要 Ctrl
    bool shift;     // 是否需要 Shift
    bool alt;       // 是否需要 Alt
};

extern HotkeyBinding g_hotkeys[HK_COUNT];

// 快捷键名称（用于设置面板显示）
const wchar_t* GetHotkeyActionName(int action);
// 将 VK 码转为可读字符串
std::wstring VKeyToString(const HotkeyBinding& hk);

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

// 快捷键编辑控件 ID（基址 3001，每个动作一个）
#define IDC_HOTKEY_BASE       3001

// ============ 资源 ID ============
#define IDI_APPICON 101

// ============ 配置持久化 ============
#define CONFIG_PATH L"C:\\GuessDraw.ini"

void LoadConfig();
void SaveConfig();
