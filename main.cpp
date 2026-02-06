#include <windows.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <thread>
#include <atomic>
#include <iostream>
#include <string>
#include <algorithm>
#include <filesystem>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

using namespace Gdiplus;
using namespace std;
namespace fs = std::filesystem;

// ============ 全局状态 ============
std::atomic<bool> running(true);
std::atomic<bool> isWindowVisible(true);
std::atomic<float> scaleFactor(1.0f);
std::atomic<float> opacityFactor(0.5f);       // 透明度 0.0~1.0
std::atomic<bool> grayscaleEnabled(false);     // 黑白化
std::atomic<bool> removeWhiteBg(false);        // 去白底
std::atomic<bool> reloadImage(false);
std::atomic<bool> autoLoadLatest(false);       // 自动加载最新图片

std::wstring currentImagePath = L"C:\\Users\\Eilo\\Pictures\\zGuess\\1.jpg";
std::wstring imageDirectory = L"C:\\Users\\Eilo\\Pictures\\zGuess";

// 窗口偏移（用于拖动）
std::atomic<int> windowOffsetX(0);
std::atomic<int> windowOffsetY(0);

HWND g_hwndMain = nullptr;
HWND g_hwndSettings = nullptr;
HINSTANCE g_hInstance = nullptr;
NOTIFYICONDATAW g_nid = {};

// ============ 常量定义 ============
#define WM_TRAYICON      (WM_USER + 1)
#define IDM_SHOW_HIDE    1001
#define IDM_SETTINGS     1002
#define IDM_RELOAD       1003
#define IDM_EXIT         1004

// 设置面板控件 ID
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

// ============ 前向声明 ============
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void DrawTransparentWindow(HWND hwnd);
void CreateTrayIcon(HWND hwnd);
void RemoveTrayIcon();
void ShowTrayMenu(HWND hwnd);
void CreateSettingsWindow();
std::wstring FindLatestImage(const std::wstring& dir);

// ============ 查找目录下最新图片 ============
std::wstring FindLatestImage(const std::wstring& dir) {
    std::wstring latestFile;
    std::filesystem::file_time_type latestTime{};
    bool found = false;

    try {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().wstring();
            // 转小写比较
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".jpg" || ext == L".jpeg" || ext == L".png" || ext == L".bmp" || ext == L".gif") {
                auto ftime = entry.last_write_time();
                if (!found || ftime > latestTime) {
                    latestTime = ftime;
                    latestFile = entry.path().wstring();
                    found = true;
                }
            }
        }
    } catch (...) {}

    return latestFile;
}

// ============ 绘制透明窗口 ============
void DrawTransparentWindow(HWND hwnd) {
    // 如果开启自动加载，先查找最新图片
    if (autoLoadLatest) {
        std::wstring latest = FindLatestImage(imageDirectory);
        if (!latest.empty()) {
            currentImagePath = latest;
        }
    }

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    Graphics graphics(hdcMem);
    graphics.Clear(Color(0, 0, 0, 0));
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    Image image(currentImagePath.c_str());
    if (image.GetLastStatus() != Ok) {
        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    UINT imgWidth = image.GetWidth();
    UINT imgHeight = image.GetHeight();

    float scale = scaleFactor.load();
    UINT renderWidth = static_cast<UINT>(imgWidth * scale);
    UINT renderHeight = static_cast<UINT>(imgHeight * scale);
    int offsetX = windowOffsetX.load() + (screenWidth - (int)renderWidth) / 2;
    int offsetY = windowOffsetY.load() + (screenHeight - (int)renderHeight) / 2;

    float alpha = opacityFactor.load();
    bool gray = grayscaleEnabled.load();
    bool rmWhite = removeWhiteBg.load();

    if (!rmWhite) {
        // 普通模式：用 ColorMatrix 控制透明度和灰度
        ColorMatrix colorMatrix;
        if (gray) {
            // 灰度 + 透明度
            colorMatrix = {
                0.299f, 0.299f, 0.299f, 0.0f, 0.0f,
                0.587f, 0.587f, 0.587f, 0.0f, 0.0f,
                0.114f, 0.114f, 0.114f, 0.0f, 0.0f,
                0.0f,   0.0f,   0.0f,   alpha, 0.0f,
                0.0f,   0.0f,   0.0f,   0.0f, 1.0f
            };
        } else {
            colorMatrix = {
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, alpha, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 1.0f
            };
        }

        ImageAttributes imageAttributes;
        imageAttributes.SetColorMatrix(&colorMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

        graphics.DrawImage(
            &image,
            Rect(offsetX, offsetY, renderWidth, renderHeight),
            0, 0, imgWidth, imgHeight,
            UnitPixel,
            &imageAttributes
        );
    } else {
        // 去白底模式：逐像素处理
        Bitmap* pBmp = dynamic_cast<Bitmap*>(&image);
        Bitmap tempBmp(imgWidth, imgHeight, PixelFormat32bppARGB);

        if (pBmp) {
            BitmapData srcData, dstData;
            Rect lockRect(0, 0, imgWidth, imgHeight);
            pBmp->LockBits(&lockRect, ImageLockModeRead, PixelFormat32bppARGB, &srcData);
            tempBmp.LockBits(&lockRect, ImageLockModeWrite, PixelFormat32bppARGB, &dstData);

            BYTE* srcPixels = (BYTE*)srcData.Scan0;
            BYTE* dstPixels = (BYTE*)dstData.Scan0;

            for (UINT y = 0; y < imgHeight; y++) {
                for (UINT x = 0; x < imgWidth; x++) {
                    int idx = y * srcData.Stride + x * 4;
                    BYTE b = srcPixels[idx + 0];
                    BYTE g = srcPixels[idx + 1];
                    BYTE r = srcPixels[idx + 2];
                    BYTE a = srcPixels[idx + 3];

                    // 判断是否接近白色（阈值 240）
                    if (r > 240 && g > 240 && b > 240) {
                        a = 0; // 完全透明
                    } else {
                        a = static_cast<BYTE>(a * alpha);
                    }

                    if (gray) {
                        BYTE grayVal = static_cast<BYTE>(0.299f * r + 0.587f * g + 0.114f * b);
                        r = g = b = grayVal;
                    }

                    dstPixels[idx + 0] = b;
                    dstPixels[idx + 1] = g;
                    dstPixels[idx + 2] = r;
                    dstPixels[idx + 3] = a;
                }
            }

            pBmp->UnlockBits(&srcData);
            tempBmp.UnlockBits(&dstData);
        }

        graphics.DrawImage(
            &tempBmp,
            Rect(offsetX, offsetY, renderWidth, renderHeight),
            0, 0, imgWidth, imgHeight,
            UnitPixel
        );
    }

    POINT ptPos = { 0, 0 };
    SIZE size = { screenWidth, screenHeight };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd, hdcScreen, nullptr, &size, hdcMem, &ptPos, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

// ============ 系统托盘 ============
void CreateTrayIcon(HWND hwnd) {
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy(g_nid.szTip, L"GuessDraw");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

void ShowTrayMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_SHOW_HIDE, isWindowVisible ? L"隐藏图片" : L"显示图片");
    AppendMenuW(hMenu, MF_STRING, IDM_RELOAD, L"重新加载");
    AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"设置");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
}

// ============ 设置面板 ============
void CreateSettingsWindow() {
    if (g_hwndSettings && IsWindow(g_hwndSettings)) {
        SetForegroundWindow(g_hwndSettings);
        return;
    }

    const wchar_t SETTINGS_CLASS[] = L"GuessDraw_Settings";
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = SettingsProc;
        wc.hInstance = g_hInstance;
        wc.lpszClassName = SETTINGS_CLASS;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        registered = true;
    }

    g_hwndSettings = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        SETTINGS_CLASS, L"GuessDraw 设置",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 380,
        nullptr, nullptr, g_hInstance, nullptr
    );

    ShowWindow(g_hwndSettings, SW_SHOW);
    UpdateWindow(g_hwndSettings);
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hSliderOpacity, hSliderScale, hCheckGray, hCheckWhite, hCheckAuto;
    static HWND hEditPath, hBtnBrowse, hLabelOpacity, hLabelScale, hBtnApply;

    switch (uMsg) {
    case WM_CREATE: {
        int y = 15;
        int labelW = 90, ctrlX = 100, ctrlW = 280;

        // 透明度
        CreateWindowW(L"STATIC", L"透明度:", WS_CHILD | WS_VISIBLE, 10, y + 2, labelW, 20, hwnd, nullptr, g_hInstance, nullptr);
        hSliderOpacity = CreateWindowW(L"msctls_trackbar32", L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            ctrlX, y, 220, 30, hwnd, (HMENU)IDC_SLIDER_OPACITY, g_hInstance, nullptr);
        SendMessage(hSliderOpacity, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessage(hSliderOpacity, TBM_SETPOS, TRUE, (int)(opacityFactor * 100));
        hLabelOpacity = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 330, y + 2, 60, 20, hwnd, (HMENU)IDC_LABEL_OPACITY, g_hInstance, nullptr);
        wchar_t buf[32];
        swprintf(buf, 32, L"%d%%", (int)(opacityFactor * 100));
        SetWindowTextW(hLabelOpacity, buf);

        y += 40;
        // 缩放
        CreateWindowW(L"STATIC", L"缩放:", WS_CHILD | WS_VISIBLE, 10, y + 2, labelW, 20, hwnd, nullptr, g_hInstance, nullptr);
        hSliderScale = CreateWindowW(L"msctls_trackbar32", L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            ctrlX, y, 220, 30, hwnd, (HMENU)IDC_SLIDER_SCALE, g_hInstance, nullptr);
        SendMessage(hSliderScale, TBM_SETRANGE, TRUE, MAKELPARAM(10, 300));
        SendMessage(hSliderScale, TBM_SETPOS, TRUE, (int)(scaleFactor * 100));
        hLabelScale = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 330, y + 2, 60, 20, hwnd, (HMENU)IDC_LABEL_SCALE, g_hInstance, nullptr);
        swprintf(buf, 32, L"%d%%", (int)(scaleFactor * 100));
        SetWindowTextW(hLabelScale, buf);

        y += 45;
        // 黑白化
        hCheckGray = CreateWindowW(L"BUTTON", L"黑白化", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            10, y, 120, 25, hwnd, (HMENU)IDC_CHECK_GRAYSCALE, g_hInstance, nullptr);
        if (grayscaleEnabled) SendMessage(hCheckGray, BM_SETCHECK, BST_CHECKED, 0);

        // 去白底
        hCheckWhite = CreateWindowW(L"BUTTON", L"去白色底", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            150, y, 120, 25, hwnd, (HMENU)IDC_CHECK_REMOVEWHITE, g_hInstance, nullptr);
        if (removeWhiteBg) SendMessage(hCheckWhite, BM_SETCHECK, BST_CHECKED, 0);

        y += 35;
        // 自动加载最新
        hCheckAuto = CreateWindowW(L"BUTTON", L"自动加载目录最新图片", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            10, y, 250, 25, hwnd, (HMENU)IDC_CHECK_AUTOLOAD, g_hInstance, nullptr);
        if (autoLoadLatest) SendMessage(hCheckAuto, BM_SETCHECK, BST_CHECKED, 0);

        y += 35;
        // 图片目录
        CreateWindowW(L"STATIC", L"图片目录:", WS_CHILD | WS_VISIBLE, 10, y + 2, labelW, 20, hwnd, nullptr, g_hInstance, nullptr);
        hEditPath = CreateWindowW(L"EDIT", imageDirectory.c_str(),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            ctrlX, y, 220, 24, hwnd, (HMENU)IDC_EDIT_PATH, g_hInstance, nullptr);
        hBtnBrowse = CreateWindowW(L"BUTTON", L"...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            330, y, 40, 24, hwnd, (HMENU)IDC_BTN_BROWSE, g_hInstance, nullptr);

        y += 35;
        // 当前图片路径（只读显示）
        CreateWindowW(L"STATIC", L"当前图片:", WS_CHILD | WS_VISIBLE, 10, y + 2, labelW, 20, hwnd, nullptr, g_hInstance, nullptr);
        HWND hCurPath = CreateWindowW(L"EDIT", currentImagePath.c_str(),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
            ctrlX, y, 290, 24, hwnd, nullptr, g_hInstance, nullptr);

        y += 45;
        // 应用按钮
        hBtnApply = CreateWindowW(L"BUTTON", L"应用并刷新", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            150, y, 120, 30, hwnd, (HMENU)IDC_BTN_APPLY, g_hInstance, nullptr);

        return 0;
    }

    case WM_HSCROLL: {
        if ((HWND)lParam == GetDlgItem(hwnd, IDC_SLIDER_OPACITY)) {
            int val = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
            opacityFactor = val / 100.0f;
            wchar_t buf[32];
            swprintf(buf, 32, L"%d%%", val);
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_OPACITY), buf);
        }
        if ((HWND)lParam == GetDlgItem(hwnd, IDC_SLIDER_SCALE)) {
            int val = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
            scaleFactor = val / 100.0f;
            wchar_t buf[32];
            swprintf(buf, 32, L"%d%%", val);
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_SCALE), buf);
        }
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == IDC_BTN_BROWSE) {
            // 打开文件夹选择对话框
            BROWSEINFOW bi = {};
            bi.hwndOwner = hwnd;
            bi.lpszTitle = L"选择图片目录";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t path[MAX_PATH];
                SHGetPathFromIDListW(pidl, path);
                imageDirectory = path;
                SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_PATH), path);
                CoTaskMemFree(pidl);
            }
        }
        if (wmId == IDC_BTN_APPLY) {
            // 读取所有设置
            grayscaleEnabled = (SendMessage(GetDlgItem(hwnd, IDC_CHECK_GRAYSCALE), BM_GETCHECK, 0, 0) == BST_CHECKED);
            removeWhiteBg = (SendMessage(GetDlgItem(hwnd, IDC_CHECK_REMOVEWHITE), BM_GETCHECK, 0, 0) == BST_CHECKED);
            autoLoadLatest = (SendMessage(GetDlgItem(hwnd, IDC_CHECK_AUTOLOAD), BM_GETCHECK, 0, 0) == BST_CHECKED);

            // 读取目录路径
            wchar_t pathBuf[MAX_PATH];
            GetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_PATH), pathBuf, MAX_PATH);
            imageDirectory = pathBuf;

            // 刷新图片
            reloadImage = true;
            PostMessage(g_hwndMain, WM_PAINT, 0, 0);
        }
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        g_hwndSettings = nullptr;
        return 0;

    case WM_DESTROY:
        g_hwndSettings = nullptr;
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ============ 快捷键监听线程 ============
void KeyListener(HWND hwnd) {
    while (running) {
        // Delete 键退出
        if (GetAsyncKeyState(VK_DELETE) & 0x8000) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;
        }

        // NumPad0 隐藏/显示
        if (GetAsyncKeyState(VK_NUMPAD0) & 0x8000) {
            if (isWindowVisible) {
                ShowWindow(hwnd, SW_HIDE);
                isWindowVisible = false;
            } else {
                ShowWindow(hwnd, SW_SHOW);
                isWindowVisible = true;
            }
            Sleep(200);
        }

        // NumPad. 重新加载
        if (GetAsyncKeyState(VK_DECIMAL) & 0x8000) {
            reloadImage = true;
            InvalidateRect(hwnd, nullptr, TRUE);
            Sleep(200);
        }

        // NumPad8 提高透明度
        if (GetAsyncKeyState(VK_NUMPAD8) & 0x8000) {
            float cur = opacityFactor.load();
            opacityFactor = min(1.0f, cur + 0.05f);
            InvalidateRect(hwnd, nullptr, TRUE);
            Sleep(100);
        }

        // NumPad2 降低透明度
        if (GetAsyncKeyState(VK_NUMPAD2) & 0x8000) {
            float cur = opacityFactor.load();
            opacityFactor = max(0.05f, cur - 0.05f);
            InvalidateRect(hwnd, nullptr, TRUE);
            Sleep(100);
        }

        // ↑ 放大图片
        if (GetAsyncKeyState(VK_UP) & 0x8000) {
            scaleFactor = scaleFactor + 0.05f;
            InvalidateRect(hwnd, nullptr, TRUE);
            Sleep(100);
        }

        // ↓ 缩小图片
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
            float cur = scaleFactor.load();
            scaleFactor = max(0.1f, cur - 0.05f);
            InvalidateRect(hwnd, nullptr, TRUE);
            Sleep(100);
        }

        // Ctrl + 鼠标左键拖动
        {
            static POINT lastPos = {0, 0};
            static bool dragging = false;

            bool ctrlDown = (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0;
            bool lbtnDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

            if (ctrlDown && lbtnDown) {
                POINT curPos;
                GetCursorPos(&curPos);

                if (!dragging) {
                    lastPos = curPos;
                    dragging = true;
                } else {
                    int dx = curPos.x - lastPos.x;
                    int dy = curPos.y - lastPos.y;
                    windowOffsetX += dx;
                    windowOffsetY += dy;
                    lastPos = curPos;
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
            } else {
                dragging = false;
            }
        }

        Sleep(10);
    }
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
            // 双击托盘图标显示/隐藏
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

    // 初始化公共控件（滑块等）
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    // 初始化 GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // 注册主窗口类
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

    // 创建托盘图标
    CreateTrayIcon(g_hwndMain);

    ShowWindow(g_hwndMain, nCmdShow);
    DrawTransparentWindow(g_hwndMain);

    // 启动快捷键监听线程
    std::thread keyListenerThread(KeyListener, g_hwndMain);

    // 消息循环
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        // 设置面板消息分发
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
