#pragma once

#include <windows.h>

// 启动区域截图流程（创建全屏覆盖窗口）
void StartScreenshot(HWND hwndMain);

// 截图覆盖窗口的消息处理函数
LRESULT CALLBACK ScreenshotProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// 注册/注销截图全局热键（阻止其他程序捕获）
void RegisterScreenshotHotkey(HWND hwnd);
void UnregisterScreenshotHotkey(HWND hwnd);
