#pragma once

#include <windows.h>
#include <string>

void DrawTransparentWindow(HWND hwnd);
std::wstring FindLatestImage(const std::wstring& dir);
