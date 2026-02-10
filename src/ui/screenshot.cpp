#include "screenshot.h"
#include "globals.h"
#include "drawing.h"
#include <gdiplus.h>
#include <string>
#include <ctime>
#include <algorithm>
#include <shlobj.h>

using namespace Gdiplus;

static const wchar_t* SCREENSHOT_CLASS = L"GuessDraw_Screenshot";
static bool s_classRegistered = false;
static HCURSOR s_hCrossCursor = nullptr;

// 截图状态
static HWND s_hwndMain = nullptr;       // 主窗口句柄
static HWND s_hwndScreenshot = nullptr; // 截图窗口句柄
static HBITMAP s_hDesktop = nullptr;     // 桌面截图
static int s_screenW = 0, s_screenH = 0;
static const UINT_PTR TIMER_CAPTURE = 1;

// 选区状态
static bool s_selecting = false;        // 正在拖拽选区
static bool s_hasSelection = false;     // 已有选区（等待确认）
static POINT s_startPt = {0, 0};        // 选区起点
static RECT s_selRect = {0, 0, 0, 0};  // 选区矩形

// 确认/取消按钮区域
static RECT s_btnConfirm = {0, 0, 0, 0};
static RECT s_btnCancel = {0, 0, 0, 0};

// 获取 PNG 编码器 CLSID
static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    ImageCodecInfo* pInfo = (ImageCodecInfo*)malloc(size);
    if (!pInfo) return -1;
    GetImageEncoders(num, size, pInfo);

    for (UINT i = 0; i < num; i++) {
        if (wcscmp(pInfo[i].MimeType, format) == 0) {
            *pClsid = pInfo[i].Clsid;
            free(pInfo);
            return i;
        }
    }
    free(pInfo);
    return -1;
}

// 规范化选区矩形（确保 left < right, top < bottom）
static RECT NormalizeRect(POINT start, POINT end) {
    RECT r;
    r.left   = std::min(start.x, end.x);
    r.top    = std::min(start.y, end.y);
    r.right  = std::max(start.x, end.x);
    r.bottom = std::max(start.y, end.y);
    return r;
}

// 计算确认/取消按钮位置（选区下方）
static void UpdateButtonRects() {
    int btnW = 70, btnH = 30, gap = 10;
    int cx = (s_selRect.left + s_selRect.right) / 2;
    int by = s_selRect.bottom + 8;

    // 如果按钮超出屏幕底部，放到选区上方
    if (by + btnH > s_screenH) {
        by = s_selRect.top - btnH - 8;
    }

    s_btnConfirm = { cx - btnW - gap / 2, by, cx - gap / 2, by + btnH };
    s_btnCancel  = { cx + gap / 2, by, cx + gap / 2 + btnW, by + btnH };
}

// 保存选区为 PNG 文件
static bool SaveSelection(HWND hwnd) {
    int w = s_selRect.right - s_selRect.left;
    int h = s_selRect.bottom - s_selRect.top;
    if (w <= 0 || h <= 0) return false;

    // 从桌面截图中裁剪选区
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcSrc = CreateCompatibleDC(hdcScreen);
    HDC hdcDst = CreateCompatibleDC(hdcScreen);
    HBITMAP hCrop = CreateCompatibleBitmap(hdcScreen, w, h);

    SelectObject(hdcSrc, s_hDesktop);
    SelectObject(hdcDst, hCrop);
    BitBlt(hdcDst, 0, 0, w, h, hdcSrc, s_selRect.left, s_selRect.top, SRCCOPY);

    // 用 GDI+ 保存为 PNG
    Bitmap bmp(hCrop, nullptr);
    CLSID pngClsid;
    bool ok = false;
    if (GetEncoderClsid(L"image/png", &pngClsid) >= 0) {
        // 生成文件名：screenshot_YYYYMMDD_HHMMSS.png
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        wchar_t filename[MAX_PATH];
        swprintf(filename, MAX_PATH, L"%ls\\screenshot_%04d%02d%02d_%02d%02d%02d.png",
                 imageDirectory.c_str(),
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                 t->tm_hour, t->tm_min, t->tm_sec);

        ok = (bmp.Save(filename, &pngClsid, nullptr) == Ok);
    }

    DeleteDC(hdcDst);
    DeleteDC(hdcSrc);
    ReleaseDC(nullptr, hdcScreen);
    DeleteObject(hCrop);
    return ok;
}

// 绘制覆盖窗口
static void PaintOverlay(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hBuf = CreateCompatibleBitmap(hdc, s_screenW, s_screenH);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBuf);

    // 绘制桌面截图
    HDC hdcDesktop = CreateCompatibleDC(hdc);
    SelectObject(hdcDesktop, s_hDesktop);
    BitBlt(hdcMem, 0, 0, s_screenW, s_screenH, hdcDesktop, 0, 0, SRCCOPY);
    DeleteDC(hdcDesktop);

    // 半透明遮罩（选区外变暗）
    {
        Graphics g(hdcMem);
        SolidBrush dimBrush(Color(120, 0, 0, 0));

        if (s_selecting || s_hasSelection) {
            // 用四个矩形覆盖选区外的区域
            RECT r = s_selRect;
            // 上
            g.FillRectangle(&dimBrush, (INT)0, (INT)0, (INT)s_screenW, (INT)r.top);
            // 下
            g.FillRectangle(&dimBrush, (INT)0, (INT)r.bottom, (INT)s_screenW, (INT)(s_screenH - r.bottom));
            // 左
            g.FillRectangle(&dimBrush, (INT)0, (INT)r.top, (INT)r.left, (INT)(r.bottom - r.top));
            // 右
            g.FillRectangle(&dimBrush, (INT)r.right, (INT)r.top, (INT)(s_screenW - r.right), (INT)(r.bottom - r.top));

            // 选区边框
            Pen borderPen(Color(255, 0, 120, 215), 2.0f);
            g.DrawRectangle(&borderPen, (INT)r.left, (INT)r.top, (INT)(r.right - r.left), (INT)(r.bottom - r.top));

            // 选区尺寸文字
            int sw = r.right - r.left;
            int sh = r.bottom - r.top;
            wchar_t sizeText[64];
            swprintf(sizeText, 64, L"%d × %d", sw, sh);
            Font font(L"Segoe UI", 11.0f);
            SolidBrush textBg(Color(180, 0, 0, 0));
            SolidBrush textBrush(Color(255, 255, 255, 255));
            RectF textRect;
            g.MeasureString(sizeText, -1, &font, PointF(0, 0), &textRect);
            float tx = (float)r.left;
            float ty = (float)r.top - textRect.Height - 4;
            if (ty < 0) ty = (float)r.top + 4;
            g.FillRectangle(&textBg, tx, ty, textRect.Width + 8, textRect.Height + 2);
            g.DrawString(sizeText, -1, &font, PointF(tx + 4, ty + 1), &textBrush);
        } else {
            // 无选区时全屏变暗
            g.FillRectangle(&dimBrush, (INT)0, (INT)0, (INT)s_screenW, (INT)s_screenH);

            // 提示文字
            Font font(L"Segoe UI", 14.0f);
            SolidBrush textBrush(Color(200, 255, 255, 255));
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            RectF area(0, 0, (float)s_screenW, (float)s_screenH);
            g.DrawString(L"拖拽鼠标选择截图区域，ESC 取消", -1, &font, area, &sf, &textBrush);
        }

        // 确认/取消按钮
        if (s_hasSelection && !s_selecting) {
            // 确认按钮
            SolidBrush confirmBg(Color(220, 0, 120, 215));
            g.FillRectangle(&confirmBg, (INT)s_btnConfirm.left, (INT)s_btnConfirm.top,
                            (INT)(s_btnConfirm.right - s_btnConfirm.left), (INT)(s_btnConfirm.bottom - s_btnConfirm.top));
            Font btnFont(L"Segoe UI", 10.0f);
            SolidBrush btnText(Color(255, 255, 255, 255));
            StringFormat btnSf;
            btnSf.SetAlignment(StringAlignmentCenter);
            btnSf.SetLineAlignment(StringAlignmentCenter);
            RectF confirmArea((float)s_btnConfirm.left, (float)s_btnConfirm.top,
                              (float)(s_btnConfirm.right - s_btnConfirm.left),
                              (float)(s_btnConfirm.bottom - s_btnConfirm.top));
            g.DrawString(L"\u2714 确认", -1, &btnFont, confirmArea, &btnSf, &btnText);

            // 取消按钮
            SolidBrush cancelBg(Color(220, 80, 80, 80));
            g.FillRectangle(&cancelBg, (INT)s_btnCancel.left, (INT)s_btnCancel.top,
                            (INT)(s_btnCancel.right - s_btnCancel.left), (INT)(s_btnCancel.bottom - s_btnCancel.top));
            RectF cancelArea((float)s_btnCancel.left, (float)s_btnCancel.top,
                             (float)(s_btnCancel.right - s_btnCancel.left),
                             (float)(s_btnCancel.bottom - s_btnCancel.top));
            g.DrawString(L"\u2716 取消", -1, &btnFont, cancelArea, &btnSf, &btnText);
        }
    }

    BitBlt(hdc, 0, 0, s_screenW, s_screenH, hdcMem, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hOld);
    DeleteObject(hBuf);
    DeleteDC(hdcMem);
    EndPaint(hwnd, &ps);
}

// 清理并关闭截图窗口
static void CloseScreenshot(HWND hwnd) {
    if (s_hDesktop) {
        DeleteObject(s_hDesktop);
        s_hDesktop = nullptr;
    }
    DestroyWindow(hwnd);
    s_hwndScreenshot = nullptr;
    // 恢复主窗口
    if (s_hwndMain && isWindowVisible) {
        ShowWindow(s_hwndMain, SW_SHOW);
    }
}

LRESULT CALLBACK ScreenshotProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                CloseScreenshot(hwnd);
                return 0;
            }
            break;

        case WM_LBUTTONDOWN: {
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };

            // 如果已有选区，检查是否点击了按钮
            if (s_hasSelection && !s_selecting) {
                if (PtInRect(&s_btnConfirm, pt)) {
                    SaveSelection(hwnd);
                    CloseScreenshot(hwnd);
                    // 触发主窗口重绘以自动加载新截图
                    reloadImage = true;
                    // PostMessage(s_hwndMain, WM_USER, 0, 0);
                    InvalidateRect(s_hwndMain, nullptr, TRUE);
                    return 0;
                }
                if (PtInRect(&s_btnCancel, pt)) {
                    CloseScreenshot(hwnd);
                    return 0;
                }
            }

            // 开始新选区
            s_selecting = true;
            s_hasSelection = false;
            s_startPt = pt;
            s_selRect = { pt.x, pt.y, pt.x, pt.y };
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (s_selecting) {
                POINT pt = { LOWORD(lParam), HIWORD(lParam) };
                s_selRect = NormalizeRect(s_startPt, pt);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (s_selecting) {
                s_selecting = false;
                ReleaseCapture();
                POINT pt = { LOWORD(lParam), HIWORD(lParam) };
                s_selRect = NormalizeRect(s_startPt, pt);

                int w = s_selRect.right - s_selRect.left;
                int h = s_selRect.bottom - s_selRect.top;
                if (w > 5 && h > 5) {
                    s_hasSelection = true;
                    UpdateButtonRects();
                } else {
                    s_hasSelection = false;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_PAINT:
            PaintOverlay(hwnd);
            return 0;

        case WM_DESTROY:
            s_selecting = false;
            s_hasSelection = false;
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// 实际执行桌面截取和创建覆盖窗口
static void DoCapture() {
    // 截取整个桌面
    s_screenW = GetSystemMetrics(SM_CXSCREEN);
    s_screenH = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    s_hDesktop = CreateCompatibleBitmap(hdcScreen, s_screenW, s_screenH);
    SelectObject(hdcMem, s_hDesktop);
    BitBlt(hdcMem, 0, 0, s_screenW, s_screenH, hdcScreen, 0, 0, SRCCOPY);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    // 创建天蓝色十字准星光标（32位ARGB，背景完全透明）
    if (!s_hCrossCursor) {
        const int sz = 32;
        const int cx = sz / 2, cy = sz / 2;
        const int lineW = 2; // 线条粗细

        // 创建 32 位 ARGB DIB
        BITMAPV5HEADER bi = {};
        bi.bV5Size        = sizeof(bi);
        bi.bV5Width       = sz;
        bi.bV5Height      = -sz; // 自上而下
        bi.bV5Planes      = 1;
        bi.bV5BitCount    = 32;
        bi.bV5Compression = BI_BITFIELDS;
        bi.bV5RedMask     = 0x00FF0000;
        bi.bV5GreenMask   = 0x0000FF00;
        bi.bV5BlueMask    = 0x000000FF;
        bi.bV5AlphaMask   = 0xFF000000;

        void* pBits = nullptr;
        HDC hdcScr = GetDC(nullptr);
        HBITMAP hbmColor = CreateDIBSection(hdcScr, (BITMAPINFO*)&bi,
                                            DIB_RGB_COLORS, &pBits, nullptr, 0);
        HBITMAP hbmMask = CreateBitmap(sz, sz, 1, 1, nullptr);

        if (pBits) {
            DWORD* pixels = (DWORD*)pBits;
            // 全部透明
            memset(pixels, 0, sz * sz * 4);

            // 天蓝色 ARGB: 0xFF00AEFF
            DWORD color = 0xFF00AEFF; // A=FF R=00 G=AE B=FF

            // 画横线
            for (int y = cy - lineW / 2; y < cy + (lineW + 1) / 2; y++) {
                if (y < 0 || y >= sz) continue;
                for (int x = 0; x < sz; x++) {
                    pixels[y * sz + x] = color;
                }
            }
            // 画竖线
            for (int x = cx - lineW / 2; x < cx + (lineW + 1) / 2; x++) {
                if (x < 0 || x >= sz) continue;
                for (int y = 0; y < sz; y++) {
                    pixels[y * sz + x] = color;
                }
            }
        }

        ICONINFO ii = {};
        ii.fIcon    = FALSE;
        ii.xHotspot = cx;
        ii.yHotspot = cy;
        ii.hbmMask  = hbmMask;
        ii.hbmColor = hbmColor;
        s_hCrossCursor = CreateIconIndirect(&ii);

        DeleteObject(hbmColor);
        DeleteObject(hbmMask);
        ReleaseDC(nullptr, hdcScr);
    }

    // 注册窗口类
    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = ScreenshotProc;
        wc.hInstance = g_hInstance;
        wc.hCursor = s_hCrossCursor ? s_hCrossCursor : LoadCursor(nullptr, IDC_CROSS);
        wc.lpszClassName = SCREENSHOT_CLASS;
        RegisterClassExW(&wc);
        s_classRegistered = true;
    }

    // 创建全屏无边框置顶窗口
    s_hwndScreenshot = CreateWindowExW(
        WS_EX_TOPMOST,
        SCREENSHOT_CLASS, L"",
        WS_POPUP | WS_VISIBLE,
        0, 0, s_screenW, s_screenH,
        nullptr, nullptr, g_hInstance, nullptr
    );

    if (s_hwndScreenshot) {
        SetForegroundWindow(s_hwndScreenshot);
        SetFocus(s_hwndScreenshot);
    }
}

void StartScreenshot(HWND hwndMain) {
    // 防止重复创建
    if (s_hwndScreenshot && IsWindow(s_hwndScreenshot)) return;

    s_hwndMain = hwndMain;
    s_selecting = false;
    s_hasSelection = false;

    // 隐藏主窗口
    ShowWindow(hwndMain, SW_HIDE);

    // 用定时器延迟 150ms 后截取桌面，避免阻塞消息循环，确保主窗口已隐藏
    SetTimer(hwndMain, TIMER_CAPTURE, 150, [](HWND hwnd, UINT, UINT_PTR id, DWORD) {
        KillTimer(hwnd, id);
        DoCapture();
    });
}
