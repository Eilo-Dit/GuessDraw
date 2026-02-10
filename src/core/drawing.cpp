#include "drawing.h"
#include "globals.h"
#include <algorithm>
#include <vector>
#include <filesystem>
#include <cmath>

using namespace Gdiplus;
namespace fs = std::filesystem;

// 扫描目录，返回修改时间最新的图片路径，同时通过 outTime 返回其时间戳
std::wstring FindLatestImage(const std::wstring& dir, std::filesystem::file_time_type* outTime = nullptr) {
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

    if (found && outTime) *outTime = latestTime;
    return latestFile;
}

// 记录已知的最新文件时间戳，只有目录中出现更新的文件时才自动切换
static std::filesystem::file_time_type s_knownLatestTime{};
static bool s_knownLatestInitialized = false;

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
}

// 强制加载目录中最新图片并更新时间戳记录
void ReloadLatestImage() {
    std::filesystem::file_time_type latestTime{};
    std::wstring latest = FindLatestImage(imageDirectory, &latestTime);
    if (!latest.empty()) {
        s_knownLatestTime = latestTime;
        s_knownLatestInitialized = true;
        currentImagePath = latest;
    }
}

// 绘制透明窗口，应用缩放/透明度/黑白化/去白底等效果
void DrawTransparentWindow(HWND hwnd) {
    if (autoLoadLatest) {
        std::filesystem::file_time_type latestTime{};
        std::wstring latest = FindLatestImage(imageDirectory, &latestTime);
        if (!latest.empty()) {
            if (!s_knownLatestInitialized) {
                // 首次初始化，加载最新图片并记录时间戳
                s_knownLatestTime = latestTime;
                s_knownLatestInitialized = true;
                currentImagePath = latest;
            } else if (latestTime > s_knownLatestTime) {
                // 目录中出现了新文件，自动切换
                s_knownLatestTime = latestTime;
                currentImagePath = latest;
            }
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
    int rotation = rotationAngle.load() % 360;

    // 原始缩放尺寸
    UINT scaledW = static_cast<UINT>(imgWidth * scale);
    UINT scaledH = static_cast<UINT>(imgHeight * scale);

    // 计算旋转后的包围盒尺寸（用于屏幕居中）
    float rad = rotation * 3.14159265f / 180.0f;
    float cosA = fabsf(cosf(rad));
    float sinA = fabsf(sinf(rad));
    UINT renderWidth  = static_cast<UINT>(scaledW * cosA + scaledH * sinA);
    UINT renderHeight = static_cast<UINT>(scaledW * sinA + scaledH * cosA);

    int offsetX = windowOffsetX.load() + (screenWidth - (int)renderWidth) / 2;
    int offsetY = windowOffsetY.load() + (screenHeight - (int)renderHeight) / 2;

    float alpha = opacityFactor.load();
    bool gray = grayscaleEnabled.load();
    bool rmWhite = removeWhiteBg.load();

    // 应用旋转：绕包围盒中心旋转
    // 绘制矩形始终用原始缩放尺寸，居中于包围盒内
    int drawX = offsetX + ((int)renderWidth - (int)scaledW) / 2;
    int drawY = offsetY + ((int)renderHeight - (int)scaledH) / 2;
    Rect drawRect(drawX, drawY, scaledW, scaledH);

    if (rotation != 0) {
        float cx = offsetX + renderWidth / 2.0f;
        float cy = offsetY + renderHeight / 2.0f;
        graphics.TranslateTransform(cx, cy);
        graphics.RotateTransform((float)rotation);
        graphics.TranslateTransform(-cx, -cy);
    }

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
            drawRect,
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
            drawRect,
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
