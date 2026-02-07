#pragma once

#include <windows.h>

void CreateSettingsWindow(); // 创建或激活设置窗口
LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam); // 设置窗口消息处理
