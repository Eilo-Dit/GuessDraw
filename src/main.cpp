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
std::atomic<int> g_dragMouseButton(VK_LBUTTON);

HWND g_hwndMain = nullptr;
HWND g_hwndSettings = nullptr;
HINSTANCE g_hInstance = nullptr;
NOTIFYICONDATAW g_nid = {};

// ============ 主窗口消息处理 ============
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
        DrawTransparentWindow(hwnd);
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowTrayMenu(hwnd);
        } else if (lParam == WM_LBUTTONUP) {
            CreateSettingsWindow();
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

    LoadConfig();

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
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
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
