#include "settings.h"
#include "globals.h"

void CreateSettingsWindow() {
    if (g_hwndSettings && IsWindow(g_hwndSettings)) {
        SetForegroundWindow(g_hwndSettings);
        return;
    }

    const wchar_t SETTINGS_CLASS[] = L"GuessDraw_Settings";
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = SettingsProc;
        wc.hInstance = g_hInstance;
        wc.lpszClassName = SETTINGS_CLASS;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        registered = true;
    }

    g_hwndSettings = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        SETTINGS_CLASS, L"GuessDraw 设置",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 380,
        nullptr, nullptr, g_hInstance, nullptr
    );

    ShowWindow(g_hwndSettings, SW_SHOW);
    UpdateWindow(g_hwndSettings);
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hSliderOpacity, hSliderScale, hCheckGray, hCheckWhite, hCheckAuto;
    static HWND hEditPath, hBtnBrowse, hLabelOpacity, hLabelScale, hBtnApply;

    switch (uMsg) {
    case WM_CREATE: {
        int y = 15;
        int labelW = 90, ctrlX = 100;

        // 透明度
        CreateWindowW(L"STATIC", L"透明度:", WS_CHILD | WS_VISIBLE, 10, y + 2, labelW, 20, hwnd, nullptr, g_hInstance, nullptr);
        hSliderOpacity = CreateWindowW(L"msctls_trackbar32", L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            ctrlX, y, 220, 30, hwnd, (HMENU)IDC_SLIDER_OPACITY, g_hInstance, nullptr);
        SendMessage(hSliderOpacity, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessage(hSliderOpacity, TBM_SETPOS, TRUE, (int)(opacityFactor * 100));
        hLabelOpacity = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 330, y + 2, 60, 20, hwnd, (HMENU)IDC_LABEL_OPACITY, g_hInstance, nullptr);
        wchar_t buf[32];
        swprintf(buf, 32, L"%d%%", (int)(opacityFactor * 100));
        SetWindowTextW(hLabelOpacity, buf);

        y += 40;
        // 缩放
        CreateWindowW(L"STATIC", L"缩放:", WS_CHILD | WS_VISIBLE, 10, y + 2, labelW, 20, hwnd, nullptr, g_hInstance, nullptr);
        hSliderScale = CreateWindowW(L"msctls_trackbar32", L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            ctrlX, y, 220, 30, hwnd, (HMENU)IDC_SLIDER_SCALE, g_hInstance, nullptr);
        SendMessage(hSliderScale, TBM_SETRANGE, TRUE, MAKELPARAM(10, 300));
        SendMessage(hSliderScale, TBM_SETPOS, TRUE, (int)(scaleFactor * 100));
        hLabelScale = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 330, y + 2, 60, 20, hwnd, (HMENU)IDC_LABEL_SCALE, g_hInstance, nullptr);
        swprintf(buf, 32, L"%d%%", (int)(scaleFactor * 100));
        SetWindowTextW(hLabelScale, buf);

        y += 45;
        // 黑白化
        hCheckGray = CreateWindowW(L"BUTTON", L"黑白化", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            10, y, 120, 25, hwnd, (HMENU)IDC_CHECK_GRAYSCALE, g_hInstance, nullptr);
        if (grayscaleEnabled) SendMessage(hCheckGray, BM_SETCHECK, BST_CHECKED, 0);

        // 去白底
        hCheckWhite = CreateWindowW(L"BUTTON", L"去白色底", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            150, y, 120, 25, hwnd, (HMENU)IDC_CHECK_REMOVEWHITE, g_hInstance, nullptr);
        if (removeWhiteBg) SendMessage(hCheckWhite, BM_SETCHECK, BST_CHECKED, 0);

        y += 35;
        // 自动加载最新
        hCheckAuto = CreateWindowW(L"BUTTON", L"自动加载目录最新图片", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            10, y, 250, 25, hwnd, (HMENU)IDC_CHECK_AUTOLOAD, g_hInstance, nullptr);
        if (autoLoadLatest) SendMessage(hCheckAuto, BM_SETCHECK, BST_CHECKED, 0);

        y += 35;
        // 图片目录
        CreateWindowW(L"STATIC", L"图片目录:", WS_CHILD | WS_VISIBLE, 10, y + 2, labelW, 20, hwnd, nullptr, g_hInstance, nullptr);
        hEditPath = CreateWindowW(L"EDIT", imageDirectory.c_str(),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            ctrlX, y, 220, 24, hwnd, (HMENU)IDC_EDIT_PATH, g_hInstance, nullptr);
        hBtnBrowse = CreateWindowW(L"BUTTON", L"...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            330, y, 40, 24, hwnd, (HMENU)IDC_BTN_BROWSE, g_hInstance, nullptr);

        y += 35;
        // 当前图片路径（只读显示）
        CreateWindowW(L"STATIC", L"当前图片:", WS_CHILD | WS_VISIBLE, 10, y + 2, labelW, 20, hwnd, nullptr, g_hInstance, nullptr);
        CreateWindowW(L"EDIT", currentImagePath.c_str(),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
            ctrlX, y, 290, 24, hwnd, nullptr, g_hInstance, nullptr);

        y += 45;
        // 应用按钮
        hBtnApply = CreateWindowW(L"BUTTON", L"应用并刷新", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            150, y, 120, 30, hwnd, (HMENU)IDC_BTN_APPLY, g_hInstance, nullptr);

        return 0;
    }

    case WM_HSCROLL: {
        if ((HWND)lParam == GetDlgItem(hwnd, IDC_SLIDER_OPACITY)) {
            int val = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
            opacityFactor = val / 100.0f;
            wchar_t buf[32];
            swprintf(buf, 32, L"%d%%", val);
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_OPACITY), buf);
        }
        if ((HWND)lParam == GetDlgItem(hwnd, IDC_SLIDER_SCALE)) {
            int val = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
            scaleFactor = val / 100.0f;
            wchar_t buf[32];
            swprintf(buf, 32, L"%d%%", val);
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_SCALE), buf);
        }
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == IDC_BTN_BROWSE) {
            BROWSEINFOW bi = {};
            bi.hwndOwner = hwnd;
            bi.lpszTitle = L"选择图片目录";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t path[MAX_PATH];
                SHGetPathFromIDListW(pidl, path);
                imageDirectory = path;
                SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_PATH), path);
                CoTaskMemFree(pidl);
            }
        }
        if (wmId == IDC_BTN_APPLY) {
            grayscaleEnabled = (SendMessage(GetDlgItem(hwnd, IDC_CHECK_GRAYSCALE), BM_GETCHECK, 0, 0) == BST_CHECKED);
            removeWhiteBg = (SendMessage(GetDlgItem(hwnd, IDC_CHECK_REMOVEWHITE), BM_GETCHECK, 0, 0) == BST_CHECKED);
            autoLoadLatest = (SendMessage(GetDlgItem(hwnd, IDC_CHECK_AUTOLOAD), BM_GETCHECK, 0, 0) == BST_CHECKED);

            wchar_t pathBuf[MAX_PATH];
            GetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_PATH), pathBuf, MAX_PATH);
            imageDirectory = pathBuf;

            reloadImage = true;
            PostMessage(g_hwndMain, WM_PAINT, 0, 0);
        }
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        g_hwndSettings = nullptr;
        return 0;

    case WM_DESTROY:
        g_hwndSettings = nullptr;
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
