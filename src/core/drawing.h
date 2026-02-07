#pragma once

#include <windows.h>
#include <string>

void DrawTransparentWindow(HWND hwnd);                  // 绘制透明叠加图片到主窗口
std::wstring FindLatestImage(const std::wstring& dir);   // 返回目录中修改时间最新的图片
void SwitchImage(int direction);                          // 切换图片 (-1=上一张, +1=下一张)
