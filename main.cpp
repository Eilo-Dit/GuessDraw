#include <windows.h>
#include <gdiplus.h>
#include <thread>
#include <atomic>
#include <iostream>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;
using namespace std;

/*
提高/降低透明度 numPad8/numPad2
放大/缩小图片 ↑ ↓
移动窗口位置  ctrl+鼠标拖动
隐藏  numPad 0
重新加载 numPad .
退出 delete
*/

//↓退出 →隐藏 0重载

std::atomic<bool> running(true);
std::atomic<bool> isWindowVisible(true);
std::atomic<float> scaleFactor(1.0f); // 图片缩放因子
std::wstring currentImagePath = L"C:\\Users\\Eilo\\Pictures\\zGuess\\1.jpg";
std::atomic<bool> reloadImage(false);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void KeyListener(HWND hwnd) {
    while (running) {
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
            PostMessage(hwnd, WM_DESTROY, 0, 0);
            break;
        }

        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
            if (isWindowVisible) {
                ShowWindow(hwnd, SW_HIDE);
                isWindowVisible = false;
            } else {
                ShowWindow(hwnd, SW_SHOW);
                isWindowVisible = true;
            }
            Sleep(200);
        }

        if (GetAsyncKeyState(VK_NUMPAD0) & 0x8000) {
            reloadImage = true;
            InvalidateRect(hwnd, nullptr, TRUE);
            Sleep(200);
        }

        Sleep(10);
    }
}

// 添加鼠标滚轮监听，调整缩放比例
void MouseListener(HWND hwnd) {
    while (running) {
        // 监听滚轮向上滚动
        if (GetAsyncKeyState(VK_NUMPAD8) & 0x8000) {
            std::cout<< "up" << std::endl;
            scaleFactor += 0.05f;
            InvalidateRect(hwnd, nullptr, TRUE);
            Sleep(100); // 防止连续触发
        }
        // 监听滚轮向下滚动
        if (GetAsyncKeyState(VK_NUMPAD2) & 0x8000) {
            std::cout<< "down" << std::endl;
            scaleFactor = max(0.1f, scaleFactor - 0.05f); // 最小缩放比例为 0.1
            InvalidateRect(hwnd, nullptr, TRUE);
            Sleep(100);
        }
        Sleep(10);
    }
}

void DrawTransparentWindow(HWND hwnd) {
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    RECT rect;
    GetClientRect(hwnd, &rect);
    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, windowWidth, windowHeight);

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    Graphics graphics(hdcMem);
    graphics.Clear(Color(0, 0, 0, 0));

    Image image(currentImagePath.c_str());
    UINT imgWidth = image.GetWidth();
    UINT imgHeight = image.GetHeight();

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    float scale = scaleFactor; // 使用全局缩放因子
    UINT renderWidth = static_cast<UINT>(imgWidth * scale);
    UINT renderHeight = static_cast<UINT>(imgHeight * scale);
    UINT offsetX = (windowWidth - renderWidth) / 2;
    UINT offsetY = (windowHeight - renderHeight) / 2;

    ColorMatrix colorMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.5f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };

    ImageAttributes imageAttributes;
    imageAttributes.SetColorMatrix(&colorMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

    graphics.DrawImage(
            &image,
            Rect(offsetX, offsetY, renderWidth, renderHeight),
            0, 0, imgWidth, imgHeight,
            UnitPixel,
            &imageAttributes
    );

    POINT ptPos = { 0, 0 };
    SIZE size = { windowWidth, windowHeight };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd, hdcScreen, nullptr, &size, hdcMem, &ptPos, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    const char CLASS_NAME[] = "TransparentWindowClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

//    int windowWidth = screenWidth / 2;
//    int windowHeight = screenHeight / 2;

//    int x = (screenWidth - windowWidth) / 2;
//    int y = (screenHeight - windowHeight) / 2;
    int x,y;
    x=y=0;
    HWND hwnd = CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
            CLASS_NAME,
            "Transparent Window",
            WS_POPUP,
            x, y, screenWidth, screenHeight,
            nullptr, nullptr, hInstance, nullptr
    );

    ShowWindow(hwnd, nCmdShow);
    DrawTransparentWindow(hwnd);

    std::thread keyListenerThread(KeyListener, hwnd);
    std::thread mouseListenerThread(MouseListener, hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (reloadImage) {
            reloadImage = false;
            DrawTransparentWindow(hwnd);
        }
    }

    running = false;
    keyListenerThread.join();
    mouseListenerThread.join();

    GdiplusShutdown(gdiplusToken);
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT:
            DrawTransparentWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
