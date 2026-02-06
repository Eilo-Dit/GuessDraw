#pragma once

#include <windows.h>

void CreateSettingsWindow();
LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
