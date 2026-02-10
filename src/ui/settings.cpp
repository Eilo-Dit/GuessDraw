#include "settings.h"
#include "globals.h"
#include <filesystem>

// 临时快捷键配置（编辑中，应用时写入 g_hotkeys）
static HotkeyBinding s_tempHotkeys[HK_COUNT];
static HWND s_hkEdits[HK_COUNT] = {};
static WNDPROC s_origEditProc = nullptr;
static HWND s_comboDragMouse = nullptr;

// 子类化 EDIT 控件，捕获按键设置快捷键
// 拖动修饰键支持单独修饰键和 Delete 清空，其他快捷键支持修饰键组合
static LRESULT CALLBACK HotkeyEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) {
        int vkey = (int)wParam;

        // 找到是哪个 hotkey edit
        int idx = -1;
        for (int i = 0; i < HK_COUNT; i++) {
            if (s_hkEdits[i] == hwnd) { idx = i; break; }
        }
        if (idx < 0) return CallWindowProcW(s_origEditProc, hwnd, uMsg, wParam, lParam);

        // 拖动修饰键特殊处理
        if (idx == HK_DRAG_MODIFIER) {
            // Delete/Backspace 清空为"无"
            if (vkey == VK_DELETE || vkey == VK_BACK) {
                s_tempHotkeys[idx].vkey = 0;
                s_tempHotkeys[idx].ctrl = false;
                s_tempHotkeys[idx].shift = false;
                s_tempHotkeys[idx].alt = false;
                SetWindowTextW(hwnd, L"无");
                return 0;
            }
            // 修饰键直接设置（不需要组合）
            int realVkey = vkey;
            // 通用修饰键转为左侧具体键
            if (vkey == VK_CONTROL) realVkey = VK_LCONTROL;
            else if (vkey == VK_SHIFT) realVkey = VK_LSHIFT;
            else if (vkey == VK_MENU) realVkey = VK_LMENU;
            s_tempHotkeys[idx].vkey = realVkey;
            s_tempHotkeys[idx].ctrl = false;
            s_tempHotkeys[idx].shift = false;
            s_tempHotkeys[idx].alt = false;
            std::wstring text = VKeyToString(s_tempHotkeys[idx]);
            SetWindowTextW(hwnd, text.c_str());
            return 0;
        }

        // 其他快捷键：忽略单独的修饰键（作为组合键的一部分）
        if (vkey == VK_CONTROL || vkey == VK_SHIFT || vkey == VK_MENU ||
            vkey == VK_LCONTROL || vkey == VK_RCONTROL ||
            vkey == VK_LSHIFT || vkey == VK_RSHIFT ||
            vkey == VK_LMENU || vkey == VK_RMENU) {
            return 0;
        }

        s_tempHotkeys[idx].vkey = vkey;
        s_tempHotkeys[idx].ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        s_tempHotkeys[idx].shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        s_tempHotkeys[idx].alt   = (GetKeyState(VK_MENU) & 0x8000) != 0;

        // 更新显示
        std::wstring text = VKeyToString(s_tempHotkeys[idx]);
        SetWindowTextW(hwnd, text.c_str());
        return 0;
    }

    if (uMsg == WM_CHAR || uMsg == WM_SYSCHAR) return 0;
    // 阻止 Alt 键激活系统菜单导致焦点丢失
    if (uMsg == WM_SYSKEYUP) return 0;
    if (uMsg == WM_SYSCOMMAND && (wParam & 0xFFF0) == SC_KEYMENU) return 0;

    return CallWindowProcW(s_origEditProc, hwnd, uMsg, wParam, lParam);
}

// 创建或激活设置窗口
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

    // 初始化临时快捷键为当前配置
    for (int i = 0; i < HK_COUNT; i++) {
        s_tempHotkeys[i] = g_hotkeys[i];
    }

    g_hwndSettings = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        SETTINGS_CLASS, L"GuessDraw 设置",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 740,
        nullptr, nullptr, g_hInstance, nullptr
    );

    ShowWindow(g_hwndSettings, SW_SHOW);
    UpdateWindow(g_hwndSettings);
}

// 设置窗口消息处理
LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hSliderOpacity, hSliderScale, hCheckGray, hCheckWhite, hCheckAuto;
    static HWND hEditPath, hBtnBrowse, hLabelOpacity, hLabelScale, hBtnApply;

    switch (uMsg) {
    case WM_CREATE: {
        int y = 15;
        int labelW = 90, ctrlX = 100;

        // ---- 图片设置区域 ----
        CreateWindowW(L"BUTTON", L" 图片设置 ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            5, y - 5, 400, 225, hwnd, nullptr, g_hInstance, nullptr);
        y += 15;

        // 透明度
        CreateWindowW(L"STATIC", L"透明度:", WS_CHILD | WS_VISIBLE, 15, y + 2, 75, 20, hwnd, nullptr, g_hInstance, nullptr);
        hSliderOpacity = CreateWindowW(L"msctls_trackbar32", L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            ctrlX, y, 220, 30, hwnd, (HMENU)IDC_SLIDER_OPACITY, g_hInstance, nullptr);
        SendMessage(hSliderOpacity, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessage(hSliderOpacity, TBM_SETPOS, TRUE, (int)(opacityFactor * 100));
        hLabelOpacity = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 330, y + 2, 60, 20, hwnd, (HMENU)IDC_LABEL_OPACITY, g_hInstance, nullptr);
        wchar_t buf[32];
        swprintf(buf, 32, L"%d%%", (int)(opacityFactor * 100));
        SetWindowTextW(hLabelOpacity, buf);

        y += 35;
        // 缩放
        CreateWindowW(L"STATIC", L"缩放:", WS_CHILD | WS_VISIBLE, 15, y + 2, 75, 20, hwnd, nullptr, g_hInstance, nullptr);
        hSliderScale = CreateWindowW(L"msctls_trackbar32", L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            ctrlX, y, 220, 30, hwnd, (HMENU)IDC_SLIDER_SCALE, g_hInstance, nullptr);
        SendMessage(hSliderScale, TBM_SETRANGE, TRUE, MAKELPARAM(10, 300));
        SendMessage(hSliderScale, TBM_SETPOS, TRUE, (int)(scaleFactor * 100));
        hLabelScale = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 330, y + 2, 60, 20, hwnd, (HMENU)IDC_LABEL_SCALE, g_hInstance, nullptr);
        swprintf(buf, 32, L"%d%%", (int)(scaleFactor * 100));
        SetWindowTextW(hLabelScale, buf);

        y += 40;
        // 黑白化 + 去白底
        hCheckGray = CreateWindowW(L"BUTTON", L"黑白化", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            15, y, 120, 25, hwnd, (HMENU)IDC_CHECK_GRAYSCALE, g_hInstance, nullptr);
        if (grayscaleEnabled) SendMessage(hCheckGray, BM_SETCHECK, BST_CHECKED, 0);
        hCheckWhite = CreateWindowW(L"BUTTON", L"去白色底", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            150, y, 120, 25, hwnd, (HMENU)IDC_CHECK_REMOVEWHITE, g_hInstance, nullptr);
        if (removeWhiteBg) SendMessage(hCheckWhite, BM_SETCHECK, BST_CHECKED, 0);

        y += 30;
        // 自动加载
        hCheckAuto = CreateWindowW(L"BUTTON", L"自动加载目录最新图片", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            15, y, 250, 25, hwnd, (HMENU)IDC_CHECK_AUTOLOAD, g_hInstance, nullptr);
        if (autoLoadLatest) SendMessage(hCheckAuto, BM_SETCHECK, BST_CHECKED, 0);

        y += 30;
        // 图片目录
        CreateWindowW(L"STATIC", L"图片目录:", WS_CHILD | WS_VISIBLE, 15, y + 2, 75, 20, hwnd, nullptr, g_hInstance, nullptr);
        hEditPath = CreateWindowW(L"EDIT", imageDirectory.c_str(),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            ctrlX, y, 220, 24, hwnd, (HMENU)IDC_EDIT_PATH, g_hInstance, nullptr);
        hBtnBrowse = CreateWindowW(L"BUTTON", L"...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            330, y, 40, 24, hwnd, (HMENU)IDC_BTN_BROWSE, g_hInstance, nullptr);

        // ---- 快捷键设置区域 ----
        y += 45;
        CreateWindowW(L"BUTTON", L" 快捷键设置（点击输入框后按键修改） ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            5, y - 5, 400, (int)(HK_COUNT * 28 + 30), hwnd, nullptr, g_hInstance, nullptr);
        y += 15;

        for (int i = 0; i < HK_COUNT; i++) {
            const wchar_t* name = GetHotkeyActionName(i);
            CreateWindowW(L"STATIC", name, WS_CHILD | WS_VISIBLE, 15, y + 2, 110, 20, hwnd, nullptr, g_hInstance, nullptr);

            std::wstring keyText = VKeyToString(s_tempHotkeys[i]);
            s_hkEdits[i] = CreateWindowW(L"EDIT", keyText.c_str(),
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_READONLY,
                130, y, 160, 24, hwnd, (HMENU)(INT_PTR)(IDC_HOTKEY_BASE + i), g_hInstance, nullptr);

            // 子类化 EDIT 控件以捕获按键
            if (!s_origEditProc) {
                s_origEditProc = (WNDPROC)SetWindowLongPtrW(s_hkEdits[i], GWLP_WNDPROC, (LONG_PTR)HotkeyEditProc);
            } else {
                SetWindowLongPtrW(s_hkEdits[i], GWLP_WNDPROC, (LONG_PTR)HotkeyEditProc);
            }

            y += 28;
        }

        // ---- 拖动鼠标键选择 ----
        y += 10;
        CreateWindowW(L"STATIC", L"拖动鼠标键:", WS_CHILD | WS_VISIBLE, 15, y + 2, 110, 20, hwnd, nullptr, g_hInstance, nullptr);
        s_comboDragMouse = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            130, y, 160, 100, hwnd, (HMENU)IDC_COMBO_DRAG_MOUSE, g_hInstance, nullptr);
        SendMessageW(s_comboDragMouse, CB_ADDSTRING, 0, (LPARAM)L"鼠标左键");
        SendMessageW(s_comboDragMouse, CB_ADDSTRING, 0, (LPARAM)L"鼠标右键");
        SendMessageW(s_comboDragMouse, CB_SETCURSEL, (g_dragMouseButton.load() == VK_RBUTTON) ? 1 : 0, 0);

        // ---- 应用 / 恢复默认 按钮 ----
        y += 35;
        hBtnApply = CreateWindowW(L"BUTTON", L"应用并刷新", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            100, y, 120, 30, hwnd, (HMENU)IDC_BTN_APPLY, g_hInstance, nullptr);
        CreateWindowW(L"BUTTON", L"恢复默认", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            230, y, 100, 30, hwnd, (HMENU)IDC_BTN_RESET, g_hInstance, nullptr);

        return 0;
    }

    case WM_HSCROLL: {
        if ((HWND)lParam == GetDlgItem(hwnd, IDC_SLIDER_OPACITY)) {
            int val = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
            opacityFactor = val / 100.0f;
            wchar_t buf[32];
            swprintf(buf, 32, L"%d%%", val);
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_OPACITY), buf);
            InvalidateRect(g_hwndMain, nullptr, TRUE);
        }
        if ((HWND)lParam == GetDlgItem(hwnd, IDC_SLIDER_SCALE)) {
            int val = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
            scaleFactor = val / 100.0f;
            wchar_t buf[32];
            swprintf(buf, 32, L"%d%%", val);
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_SCALE), buf);
            InvalidateRect(g_hwndMain, nullptr, TRUE);
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
        if (wmId == IDC_BTN_RESET) {
            // 恢复默认快捷键
            static const HotkeyBinding defaults[HK_COUNT] = {
                { VK_END,     false, false, false },
                { VK_NUMPAD0, false, false, false },
                { VK_DECIMAL, false, false, false },
                { VK_NUMPAD8, false, false, false },
                { VK_NUMPAD2, false, false, false },
                { VK_UP,      false, false, false },
                { VK_DOWN,    false, false, false },
                { VK_LCONTROL,false, false, false },
                { VK_LEFT,    false, false, false },
                { VK_RIGHT,   false, false, false },
                { VK_NUMPAD6, false, false, false },
                { VK_NUMPAD4, false, false, false },
            };
            for (int i = 0; i < HK_COUNT; i++) {
                s_tempHotkeys[i] = defaults[i];
                std::wstring text = VKeyToString(s_tempHotkeys[i]);
                SetWindowTextW(s_hkEdits[i], text.c_str());
            }

            // 恢复默认图片设置
            SendMessage(GetDlgItem(hwnd, IDC_SLIDER_OPACITY), TBM_SETPOS, TRUE, 50);
            opacityFactor = 0.5f;
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_OPACITY), L"50%");

            SendMessage(GetDlgItem(hwnd, IDC_SLIDER_SCALE), TBM_SETPOS, TRUE, 50);
            scaleFactor = 0.5f;
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_SCALE), L"50%");

            SendMessage(GetDlgItem(hwnd, IDC_CHECK_GRAYSCALE), BM_SETCHECK, BST_UNCHECKED, 0);
            grayscaleEnabled = false;
            SendMessage(GetDlgItem(hwnd, IDC_CHECK_REMOVEWHITE), BM_SETCHECK, BST_UNCHECKED, 0);
            removeWhiteBg = false;
            SendMessage(GetDlgItem(hwnd, IDC_CHECK_AUTOLOAD), BM_SETCHECK, BST_CHECKED, 0);
            autoLoadLatest = true;

            // 恢复拖动鼠标键默认
            SendMessageW(s_comboDragMouse, CB_SETCURSEL, 0, 0);
            g_dragMouseButton = VK_LBUTTON;

            // 恢复旋转角度
            rotationAngle = 0;

            InvalidateRect(g_hwndMain, nullptr, TRUE);
        }
        if (wmId == IDC_BTN_APPLY) {
            // 应用图片设置
            grayscaleEnabled = (SendMessage(GetDlgItem(hwnd, IDC_CHECK_GRAYSCALE), BM_GETCHECK, 0, 0) == BST_CHECKED);
            removeWhiteBg = (SendMessage(GetDlgItem(hwnd, IDC_CHECK_REMOVEWHITE), BM_GETCHECK, 0, 0) == BST_CHECKED);
            autoLoadLatest = (SendMessage(GetDlgItem(hwnd, IDC_CHECK_AUTOLOAD), BM_GETCHECK, 0, 0) == BST_CHECKED);

            wchar_t pathBuf[MAX_PATH];
            GetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_PATH), pathBuf, MAX_PATH);
            std::wstring newDir = pathBuf;

            // 目录变化时迁移配置文件
            if (newDir != imageDirectory) {
                std::wstring oldConfig = std::wstring(GetConfigPath());
                imageDirectory = newDir;
                std::filesystem::create_directories(imageDirectory);
                std::wstring newConfig = std::wstring(GetConfigPath());
                try {
                    if (std::filesystem::exists(oldConfig)) {
                        std::filesystem::rename(oldConfig, newConfig);
                    }
                } catch (...) {}
            }

            // 应用快捷键设置
            for (int i = 0; i < HK_COUNT; i++) {
                g_hotkeys[i] = s_tempHotkeys[i];
            }

            // 应用拖动鼠标键
            int sel = (int)SendMessageW(s_comboDragMouse, CB_GETCURSEL, 0, 0);
            g_dragMouseButton = (sel == 1) ? VK_RBUTTON : VK_LBUTTON;

            // 保存配置到文件
            SaveConfig();

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
