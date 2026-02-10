#include "hotkeys.h"
#include "globals.h"
#include "drawing.h"
#include "screenshot.h"

using namespace std;

// 检查快捷键是否被按下（主键 + 修饰键全部满足）
static bool IsHotkeyPressed(const HotkeyBinding& hk) {
    if (!(GetAsyncKeyState(hk.vkey) & 0x8000)) return false;
    if (hk.ctrl  && !(GetAsyncKeyState(VK_CONTROL) & 0x8000)) return false;
    if (hk.shift && !(GetAsyncKeyState(VK_SHIFT) & 0x8000))   return false;
    if (hk.alt   && !(GetAsyncKeyState(VK_MENU) & 0x8000))    return false;
    return true;
}

// 快捷键监听线程，轮询检测按键并触发对应动作
void KeyListener(HWND hwnd) {
    while (running) {
        // 退出
        if (IsHotkeyPressed(g_hotkeys[HK_EXIT])) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;
        }

        // 显示/隐藏
        if (IsHotkeyPressed(g_hotkeys[HK_TOGGLE_VISIBLE])) {
            if (isWindowVisible) {
                ShowWindow(hwnd, SW_HIDE);
                isWindowVisible = false;
            } else {
                ShowWindow(hwnd, SW_SHOW);
                isWindowVisible = true;
            }
            Sleep(200);
        }

        // 重新加载（强制加载目录中最新图片）
        if (IsHotkeyPressed(g_hotkeys[HK_RELOAD])) {
            ReloadLatestImage();
            reloadImage = true;
            PostMessage(hwnd, WM_USER, 0, 0);
            Sleep(200);
        }

        // 提高透明度
        if (IsHotkeyPressed(g_hotkeys[HK_OPACITY_UP])) {
            float cur = opacityFactor.load();
            opacityFactor = min(1.0f, cur + 0.05f);
            InvalidateRect(hwnd, nullptr, TRUE);
            Sleep(100);
        }

        // 降低透明度
        if (IsHotkeyPressed(g_hotkeys[HK_OPACITY_DOWN])) {
            float cur = opacityFactor.load();
            opacityFactor = max(0.05f, cur - 0.05f);
            InvalidateRect(hwnd, nullptr, TRUE);
            Sleep(100);
        }

        // 放大图片
        if (IsHotkeyPressed(g_hotkeys[HK_SCALE_UP])) {
            scaleFactor = scaleFactor + 0.05f;
            InvalidateRect(hwnd, nullptr, TRUE);
            Sleep(100);
        }

        // 缩小图片
        if (IsHotkeyPressed(g_hotkeys[HK_SCALE_DOWN])) {
            float cur = scaleFactor.load();
            scaleFactor = max(0.1f, cur - 0.05f);
            InvalidateRect(hwnd, nullptr, TRUE);
            Sleep(100);
        }

        // 上一张图片（边沿检测：仅在按键从未按下变为按下时触发）
        {
            static bool prevDown = false;
            bool nowDown = IsHotkeyPressed(g_hotkeys[HK_PREV_IMAGE]);
            if (nowDown && !prevDown) {
                SwitchImage(-1);
                reloadImage = true;
                PostMessage(hwnd, WM_USER, 0, 0);
            }
            prevDown = nowDown;
        }

        // 下一张图片（边沿检测）
        {
            static bool nextDown = false;
            bool nowDown = IsHotkeyPressed(g_hotkeys[HK_NEXT_IMAGE]);
            if (nowDown && !nextDown) {
                SwitchImage(1);
                reloadImage = true;
                PostMessage(hwnd, WM_USER, 0, 0);
            }
            nextDown = nowDown;
        }

        // 顺时针旋转（边沿检测）
        {
            static bool cwDown = false;
            bool nowDown = IsHotkeyPressed(g_hotkeys[HK_ROTATE_CW]);
            if (nowDown && !cwDown) {
                rotationAngle = (rotationAngle.load() + 10) % 360;
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            cwDown = nowDown;
        }

        // 逆时针旋转（边沿检测）
        {
            static bool ccwDown = false;
            bool nowDown = IsHotkeyPressed(g_hotkeys[HK_ROTATE_CCW]);
            if (nowDown && !ccwDown) {
                rotationAngle = (rotationAngle.load() + 350) % 360;
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            ccwDown = nowDown;
        }

        // 区域截图（边沿检测）
        {
            static bool ssDown = false;
            bool nowDown = IsHotkeyPressed(g_hotkeys[HK_SCREENSHOT]);
            if (nowDown && !ssDown) {
                StartScreenshot(hwnd);
            }
            ssDown = nowDown;
        }

        // 拖动：修饰键(vkey==0表示无修饰键) + 鼠标键
        {
            static POINT lastPos = {0, 0};
            static bool dragging = false;

            int modVkey = g_hotkeys[HK_DRAG_MODIFIER].vkey;
            bool modDown = (modVkey == 0) || (GetAsyncKeyState(modVkey) & 0x8000) != 0;
            bool mbtnDown = (GetAsyncKeyState(g_dragMouseButton.load()) & 0x8000) != 0;

            if (modDown && mbtnDown) {
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
