#include "globals.h"
#include "drawing.h"
#include "tray.h"
#include "settings.h"
#include "hotkeys.h"
#include <thread>

using namespace Gdiplus;

// ============ 全局变量定义 ============
std::atomic<bool> running(true);
std::atomic<bool> isWindowVisible(true);
std::atomic<float> scaleFactor(1.0f);
std::atomic<float> opacityFactor(0.5f);
std::atomic<bool> grayscaleEnabled(false);
std::atomic<bool> removeWhiteBg(false);
std::atomic<bool> reloadImage(false);
std::atomic<bool> autoLoadLatest(false);

std::wstring currentImagePath = L"C:\\Users\\Eilo\\Pictures\\zGuess\\1.jpg";
std::wstring imageDirectory = L"C:\\Users\\Eilo\\Pictures\\zGuess";

std::atomic<int> windowOffsetX(0);
std::atomic<int> windowOffsetY(0);

HWND g_hwndMain = nullptr;
HWND g_hwndSettings = nullptr;
HINSTANCE g_hInstance = nullptr;
NOTIFYICONDATAW g_nid = {};

// ============ 快捷键默认配置 ============
HotkeyBinding g_hotkeys[HK_COUNT] = {
    { VK_DELETE,  false, false, false },  // HK_EXIT
    { VK_NUMPAD0, false, false, false },  // HK_TOGGLE_VISIBLE
    { VK_DECIMAL, false, false, false },  // HK_RELOAD
    { VK_NUMPAD8, false, false, false },  // HK_OPACITY_UP
    { VK_NUMPAD2, false, false, false },  // HK_OPACITY_DOWN
    { VK_UP,      false, false, false },  // HK_SCALE_UP
    { VK_DOWN,    false, false, false },  // HK_SCALE_DOWN
    { VK_LCONTROL,false, false, false },  // HK_DRAG_MODIFIER
};

const wchar_t* GetHotkeyActionName(int action) {
    static const wchar_t* names[] = {
        L"退出程序",
        L"显示/隐藏",
        L"重新加载",
        L"提高透明度",
        L"降低透明度",
        L"放大图片",
        L"缩小图片",
        L"拖动修饰键",
    };
    if (action >= 0 && action < HK_COUNT) return names[action];
    return L"未知";
}

std::wstring VKeyToString(const HotkeyBinding& hk) {
    std::wstring result;
    if (hk.ctrl)  result += L"Ctrl+";
    if (hk.shift) result += L"Shift+";
    if (hk.alt)   result += L"Alt+";

    // 获取按键名称
    UINT scanCode = MapVirtualKeyW(hk.vkey, MAPVK_VK_TO_VSC);
    wchar_t keyName[64] = {};

    // 特殊键需要扩展标志
    bool extended = (hk.vkey == VK_UP || hk.vkey == VK_DOWN || hk.vkey == VK_LEFT || hk.vkey == VK_RIGHT ||
                     hk.vkey == VK_DELETE || hk.vkey == VK_INSERT || hk.vkey == VK_HOME || hk.vkey == VK_END ||
                     hk.vkey == VK_PRIOR || hk.vkey == VK_NEXT);
    if (extended) scanCode |= 0x100;

    if (GetKeyNameTextW(scanCode << 16, keyName, 64) > 0) {
        result += keyName;
    } else {
        // 手动处理一些常见键
        switch (hk.vkey) {
            case VK_LCONTROL: result += L"LCtrl"; break;
            case VK_RCONTROL: result += L"RCtrl"; break;
            case VK_LSHIFT:   result += L"LShift"; break;
            case VK_LMENU:    result += L"LAlt"; break;
            case VK_NUMPAD0:  result += L"Num0"; break;
            case VK_NUMPAD1:  result += L"Num1"; break;
            case VK_NUMPAD2:  result += L"Num2"; break;
            case VK_NUMPAD3:  result += L"Num3"; break;
            case VK_NUMPAD4:  result += L"Num4"; break;
            case VK_NUMPAD5:  result += L"Num5"; break;
            case VK_NUMPAD6:  result += L"Num6"; break;
            case VK_NUMPAD7:  result += L"Num7"; break;
            case VK_NUMPAD8:  result += L"Num8"; break;
            case VK_NUMPAD9:  result += L"Num9"; break;
            case VK_DECIMAL:  result += L"Num."; break;
            default: {
                wchar_t tmp[16];
                swprintf(tmp, 16, L"VK_%d", hk.vkey);
                result += tmp;
            }
        }
    }
    return result;
}

// ============ 主窗口消息处理 ============
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
        DrawTransparentWindow(hwnd);
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowTrayMenu(hwnd);
        } else if (lParam == WM_LBUTTONDBLCLK) {
            if (isWindowVisible) {
                ShowWindow(hwnd, SW_HIDE);
                isWindowVisible = false;
            } else {
                ShowWindow(hwnd, SW_SHOW);
                isWindowVisible = true;
            }
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_SHOW_HIDE:
            if (isWindowVisible) {
                ShowWindow(hwnd, SW_HIDE);
                isWindowVisible = false;
            } else {
                ShowWindow(hwnd, SW_SHOW);
                isWindowVisible = true;
            }
            break;
        case IDM_RELOAD:
            reloadImage = true;
            DrawTransparentWindow(hwnd);
            break;
        case IDM_SETTINGS:
            CreateSettingsWindow();
            break;
        case IDM_EXIT:
            RemoveTrayIcon();
            running = false;
            PostQuitMessage(0);
            break;
        }
        return 0;

    case WM_CLOSE:
        RemoveTrayIcon();
        running = false;
        PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ============ 主函数 ============
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInstance = hInstance;

    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    const wchar_t CLASS_NAME[] = L"GuessDraw_Main";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    g_hwndMain = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
        CLASS_NAME,
        L"GuessDraw",
        WS_POPUP,
        0, 0, screenWidth, screenHeight,
        nullptr, nullptr, hInstance, nullptr
    );

    CreateTrayIcon(g_hwndMain);
    ShowWindow(g_hwndMain, nCmdShow);
    DrawTransparentWindow(g_hwndMain);

    std::thread keyListenerThread(KeyListener, g_hwndMain);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (g_hwndSettings && IsDialogMessage(g_hwndSettings, &msg)) continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (reloadImage) {
            reloadImage = false;
            DrawTransparentWindow(g_hwndMain);
        }
    }

    running = false;
    keyListenerThread.join();

    RemoveTrayIcon();
    GdiplusShutdown(gdiplusToken);
    return 0;
}
