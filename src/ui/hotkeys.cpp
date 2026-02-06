#include "hotkeys.h"
#include "globals.h"

using namespace std;

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
