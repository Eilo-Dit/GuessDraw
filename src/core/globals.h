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
extern std::atomic<bool> running;          // 程序运行标志
extern std::atomic<bool> isWindowVisible;  // 主窗口是否可见
extern std::atomic<float> scaleFactor;     // 图片缩放比例 (0.1~3.0)
extern std::atomic<float> opacityFactor;   // 图片透明度 (0.0~1.0)
extern std::atomic<bool> grayscaleEnabled; // 黑白化开关
extern std::atomic<bool> removeWhiteBg;    // 去白底开关
extern std::atomic<bool> reloadImage;      // 触发重绘标志
extern std::atomic<bool> autoLoadLatest;   // 自动加载目录最新图片

extern std::wstring currentImagePath;      // 当前显示的图片路径
extern std::wstring imageDirectory;        // 图片目录

extern std::atomic<int> windowOffsetX;     // 拖动偏移 X
extern std::atomic<int> windowOffsetY;     // 拖动偏移 Y
extern std::atomic<int> g_dragMouseButton; // 拖动鼠标键 (VK_LBUTTON/VK_RBUTTON)

extern HWND g_hwndMain;                    // 主窗口句柄
extern HWND g_hwndSettings;                // 设置窗口句柄
extern HINSTANCE g_hInstance;              // 程序实例句柄
extern NOTIFYICONDATAW g_nid;              // 托盘图标数据

// ============ 托盘菜单命令 ============
#define WM_TRAYICON      (WM_USER + 1)
#define IDM_SHOW_HIDE    1001
#define IDM_SETTINGS     1002
#define IDM_RELOAD       1003
#define IDM_EXIT         1004

// ============ 设置面板控件 ID ============
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
#define IDC_COMBO_DRAG_MOUSE  2011
#define IDC_BTN_RESET         2012
#define IDC_HOTKEY_BASE       3001 // 快捷键编辑控件基址，每个动作 +1

// ============ 资源 ID ============
#define IDI_APPICON 101

// ============ 配置持久化 ============
const wchar_t* GetConfigPath(); // 返回 INI 文件完整路径
void LoadConfig();               // 从 INI 加载配置，首次运行自动生成
void SaveConfig();               // 保存当前配置到 INI
