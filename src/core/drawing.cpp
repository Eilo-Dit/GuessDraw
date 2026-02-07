#include "drawing.h"
#include "globals.h"
#include <algorithm>
#include <vector>
#include <filesystem>

using namespace Gdiplus;
namespace fs = std::filesystem;

// 扫描目录，返回修改时间最新的图片路径
std::wstring FindLatestImage(const std::wstring& dir) {
    std::wstring latestFile;
    std::filesystem::file_time_type latestTime{};
    bool found = false;

    try {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".jpg" || ext == L".jpeg" || ext == L".png" || ext == L".bmp" ||
                ext == L".gif" || ext == L".tiff" || ext == L".tif" || ext == L".ico" || ext == L".webp") {
                auto ftime = entry.last_write_time();
                if (!found || ftime > latestTime) {
                    latestTime = ftime;
                    latestFile = entry.path().wstring();
                    found = true;
                }
            }
        }
    } catch (...) {}

    return latestFile;
}

static bool s_manualSwitch = false; // 手动切换标志，跳过一次自动加载

// 切换到上/下一张图片 (direction: -1=上一张, +1=下一张)
void SwitchImage(int direction) {
    std::vector<std::wstring> images;
    try {
        for (const auto& entry : fs::directory_iterator(imageDirectory)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".jpg" || ext == L".jpeg" || ext == L".png" || ext == L".bmp" ||
                ext == L".gif" || ext == L".tiff" || ext == L".tif" || ext == L".ico" || ext == L".webp") {
                images.push_back(entry.path().wstring());
            }
        }
    } catch (...) { return; }

    if (images.empty()) return;
    std::sort(images.begin(), images.end());

    // 找到当前图片的位置
    int curIdx = -1;
    for (int i = 0; i < (int)images.size(); i++) {
        if (images[i] == currentImagePath) { curIdx = i; break; }
    }

    // 切换
    int newIdx;
    if (curIdx < 0) {
        newIdx = (direction > 0) ? 0 : (int)images.size() - 1;
    } else {
        newIdx = curIdx + direction;
        if (newIdx < 0) newIdx = (int)images.size() - 1;
        if (newIdx >= (int)images.size()) newIdx = 0;
    }
    currentImagePath = images[newIdx];
    s_manualSwitch = true;
}

// 绘制透明窗口，应用缩放/透明度/黑白化/去白底等效果
void DrawTransparentWindow(HWND hwnd) {
    if (s_manualSwitch) {
        s_manualSwitch = false;
    } else if (autoLoadLatest) {
        std::wstring latest = FindLatestImage(imageDirectory);
        if (!latest.empty()) {
            currentImagePath = latest;
        }
    }

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    Graphics graphics(hdcMem);
    graphics.Clear(Color(0, 0, 0, 0));
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    Bitmap image(currentImagePath.c_str());
    if (image.GetLastStatus() != Ok) {
        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    UINT imgWidth = image.GetWidth();
    UINT imgHeight = image.GetHeight();

    float scale = scaleFactor.load();
    UINT renderWidth = static_cast<UINT>(imgWidth * scale);
    UINT renderHeight = static_cast<UINT>(imgHeight * scale);
    int offsetX = windowOffsetX.load() + (screenWidth - (int)renderWidth) / 2;
    int offsetY = windowOffsetY.load() + (screenHeight - (int)renderHeight) / 2;

    float alpha = opacityFactor.load();
    bool gray = grayscaleEnabled.load();
    bool rmWhite = removeWhiteBg.load();

    if (!rmWhite) {
        // 通过颜色矩阵实现透明度和黑白化
        ColorMatrix colorMatrix;
        if (gray) {
            colorMatrix = {
                0.299f, 0.299f, 0.299f, 0.0f, 0.0f,
                0.587f, 0.587f, 0.587f, 0.0f, 0.0f,
                0.114f, 0.114f, 0.114f, 0.0f, 0.0f,
                0.0f,   0.0f,   0.0f,   alpha, 0.0f,
                0.0f,   0.0f,   0.0f,   0.0f, 1.0f
            };
        } else {
            colorMatrix = {
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, alpha, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 1.0f
            };
        }

        ImageAttributes imageAttributes;
        imageAttributes.SetColorMatrix(&colorMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

        graphics.DrawImage(
            &image,
            Rect(offsetX, offsetY, renderWidth, renderHeight),
            0, 0, imgWidth, imgHeight,
            UnitPixel,
            &imageAttributes
        );
    } else {
        // 去白底模式：逐像素处理，白色区域设为全透明
        Bitmap tempBmp(imgWidth, imgHeight, PixelFormat32bppARGB);
        {
            BitmapData srcData, dstData;
            Rect lockRect(0, 0, imgWidth, imgHeight);
            image.LockBits(&lockRect, ImageLockModeRead, PixelFormat32bppARGB, &srcData);
            tempBmp.LockBits(&lockRect, ImageLockModeWrite, PixelFormat32bppARGB, &dstData);

            BYTE* srcPixels = (BYTE*)srcData.Scan0;
            BYTE* dstPixels = (BYTE*)dstData.Scan0;

            for (UINT y = 0; y < imgHeight; y++) {
                for (UINT x = 0; x < imgWidth; x++) {
                    int idx = y * srcData.Stride + x * 4;
                    BYTE b = srcPixels[idx + 0];
                    BYTE g = srcPixels[idx + 1];
                    BYTE r = srcPixels[idx + 2];
                    BYTE a = srcPixels[idx + 3];

                    if (r > 240 && g > 240 && b > 240) {
                        a = 0;
                    } else {
                        a = static_cast<BYTE>(a * alpha);
                    }

                    if (gray) {
                        BYTE grayVal = static_cast<BYTE>(0.299f * r + 0.587f * g + 0.114f * b);
                        r = g = b = grayVal;
                    }

                    dstPixels[idx + 0] = b;
                    dstPixels[idx + 1] = g;
                    dstPixels[idx + 2] = r;
                    dstPixels[idx + 3] = a;
                }
            }

            image.UnlockBits(&srcData);
            tempBmp.UnlockBits(&dstData);
        }

        graphics.DrawImage(
            &tempBmp,
            Rect(offsetX, offsetY, renderWidth, renderHeight),
            0, 0, imgWidth, imgHeight,
            UnitPixel
        );
    }

    // 用 UpdateLayeredWindow 将内存 DC 刷新到分层窗口
    POINT ptPos = { 0, 0 };
    SIZE size = { screenWidth, screenHeight };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd, hdcScreen, nullptr, &size, hdcMem, &ptPos, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}
