#pragma once

#include <windows.h>
#include <string>

void DrawTransparentWindow(HWND hwnd);
std::wstring FindLatestImage(const std::wstring& dir);
void SwitchImage(int direction); // -1=上一张, +1=下一张
