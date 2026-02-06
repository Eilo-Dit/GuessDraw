#include "globals.h"

// ============ 快捷键默认配置 ============
HotkeyBinding g_hotkeys[HK_COUNT] = {
    { VK_END,     false, false, false },  // HK_EXIT
    { VK_NUMPAD0, false, false, false },  // HK_TOGGLE_VISIBLE
    { VK_DECIMAL, false, false, false },  // HK_RELOAD
    { VK_NUMPAD8, false, false, false },  // HK_OPACITY_UP
    { VK_NUMPAD2, false, false, false },  // HK_OPACITY_DOWN
    { VK_UP,      false, false, false },  // HK_SCALE_UP
    { VK_DOWN,    false, false, false },  // HK_SCALE_DOWN
    { VK_LCONTROL,false, false, false },  // HK_DRAG_MODIFIER
    { VK_LEFT,    false, false, false },  // HK_PREV_IMAGE
    { VK_RIGHT,   false, false, false },  // HK_NEXT_IMAGE
};

const wchar_t* GetHotkeyActionName(int action) {
    static const wchar_t* names[] = {
        L"退出程序",
        L"显示/隐藏",
        L"重新加载",
        L"提高透明度",
        L"降低透明度",
        L"放大图片",
        L"缩小图片",
        L"拖动修饰键",
        L"上一张图片",
        L"下一张图片",
    };
    if (action >= 0 && action < HK_COUNT) return names[action];
    return L"未知";
}

std::wstring VKeyToString(const HotkeyBinding& hk) {
    std::wstring result;
    if (hk.ctrl)  result += L"Ctrl+";
    if (hk.shift) result += L"Shift+";
    if (hk.alt)   result += L"Alt+";

    UINT scanCode = MapVirtualKeyW(hk.vkey, MAPVK_VK_TO_VSC);
    wchar_t keyName[64] = {};

    bool extended = (hk.vkey == VK_UP || hk.vkey == VK_DOWN || hk.vkey == VK_LEFT || hk.vkey == VK_RIGHT ||
                     hk.vkey == VK_DELETE || hk.vkey == VK_INSERT || hk.vkey == VK_HOME || hk.vkey == VK_END ||
                     hk.vkey == VK_PRIOR || hk.vkey == VK_NEXT);
    if (extended) scanCode |= 0x100;

    if (GetKeyNameTextW(scanCode << 16, keyName, 64) > 0) {
        result += keyName;
    } else {
        switch (hk.vkey) {
            case VK_LCONTROL: result += L"LCtrl"; break;
            case VK_RCONTROL: result += L"RCtrl"; break;
            case VK_LSHIFT:   result += L"LShift"; break;
            case VK_LMENU:    result += L"LAlt"; break;
            case VK_NUMPAD0:  result += L"Num0"; break;
            case VK_NUMPAD1:  result += L"Num1"; break;
            case VK_NUMPAD2:  result += L"Num2"; break;
            case VK_NUMPAD3:  result += L"Num3"; break;
            case VK_NUMPAD4:  result += L"Num4"; break;
            case VK_NUMPAD5:  result += L"Num5"; break;
            case VK_NUMPAD6:  result += L"Num6"; break;
            case VK_NUMPAD7:  result += L"Num7"; break;
            case VK_NUMPAD8:  result += L"Num8"; break;
            case VK_NUMPAD9:  result += L"Num9"; break;
            case VK_DECIMAL:  result += L"Num."; break;
            default: {
                wchar_t tmp[16];
                swprintf(tmp, 16, L"VK_%d", hk.vkey);
                result += tmp;
            }
        }
    }
    return result;
}

// ============ 配置读写 ============
const wchar_t* GetConfigPath() {
    static wchar_t path[MAX_PATH] = {};
    if (path[0] == L'\0') {
        wchar_t appdata[MAX_PATH];
        if (GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH) > 0) {
            swprintf(path, MAX_PATH, L"%s\\GuessDraw.ini", appdata);
        } else {
            wcscpy(path, L"C:\\GuessDraw.ini");
        }
    }
    return path;
}

static const wchar_t* s_hotkeyKeys[] = {
    L"Exit", L"ToggleVisible", L"Reload",
    L"OpacityUp", L"OpacityDown",
    L"ScaleUp", L"ScaleDown", L"DragModifier",
    L"PrevImage", L"NextImage"
};

void LoadConfig() {
    // 如果配置文件不存在，使用默认值
    if (GetFileAttributesW(GetConfigPath()) == INVALID_FILE_ATTRIBUTES) return;

    wchar_t buf[MAX_PATH];

    // [Image]
    GetPrivateProfileStringW(L"Image", L"Directory", imageDirectory.c_str(), buf, MAX_PATH, GetConfigPath());
    imageDirectory = buf;
    GetPrivateProfileStringW(L"Image", L"ImagePath", currentImagePath.c_str(), buf, MAX_PATH, GetConfigPath());
    currentImagePath = buf;

    opacityFactor = GetPrivateProfileIntW(L"Image", L"Opacity", 50, GetConfigPath()) / 100.0f;
    scaleFactor   = GetPrivateProfileIntW(L"Image", L"Scale", 100, GetConfigPath()) / 100.0f;
    grayscaleEnabled = GetPrivateProfileIntW(L"Image", L"Grayscale", 0, GetConfigPath()) != 0;
    removeWhiteBg    = GetPrivateProfileIntW(L"Image", L"RemoveWhite", 0, GetConfigPath()) != 0;
    autoLoadLatest   = GetPrivateProfileIntW(L"Image", L"AutoLoad", 0, GetConfigPath()) != 0;

    // [Hotkeys]
    for (int i = 0; i < HK_COUNT; i++) {
        g_hotkeys[i].vkey  = GetPrivateProfileIntW(L"Hotkeys", s_hotkeyKeys[i], g_hotkeys[i].vkey, GetConfigPath());
        wchar_t modKey[64];
        swprintf(modKey, 64, L"%s_Mod", s_hotkeyKeys[i]);
        int mod = GetPrivateProfileIntW(L"Hotkeys", modKey, 0, GetConfigPath());
        g_hotkeys[i].ctrl  = (mod & 1) != 0;
        g_hotkeys[i].shift = (mod & 2) != 0;
        g_hotkeys[i].alt   = (mod & 4) != 0;
    }
}

void SaveConfig() {
    wchar_t buf[MAX_PATH];

    // [Image]
    WritePrivateProfileStringW(L"Image", L"Directory", imageDirectory.c_str(), GetConfigPath());
    WritePrivateProfileStringW(L"Image", L"ImagePath", currentImagePath.c_str(), GetConfigPath());

    swprintf(buf, MAX_PATH, L"%d", (int)(opacityFactor * 100));
    WritePrivateProfileStringW(L"Image", L"Opacity", buf, GetConfigPath());
    swprintf(buf, MAX_PATH, L"%d", (int)(scaleFactor * 100));
    WritePrivateProfileStringW(L"Image", L"Scale", buf, GetConfigPath());
    swprintf(buf, MAX_PATH, L"%d", (int)grayscaleEnabled.load());
    WritePrivateProfileStringW(L"Image", L"Grayscale", buf, GetConfigPath());
    swprintf(buf, MAX_PATH, L"%d", (int)removeWhiteBg.load());
    WritePrivateProfileStringW(L"Image", L"RemoveWhite", buf, GetConfigPath());
    swprintf(buf, MAX_PATH, L"%d", (int)autoLoadLatest.load());
    WritePrivateProfileStringW(L"Image", L"AutoLoad", buf, GetConfigPath());

    // [Hotkeys]
    for (int i = 0; i < HK_COUNT; i++) {
        swprintf(buf, MAX_PATH, L"%d", g_hotkeys[i].vkey);
        WritePrivateProfileStringW(L"Hotkeys", s_hotkeyKeys[i], buf, GetConfigPath());

        wchar_t modKey[64];
        swprintf(modKey, 64, L"%s_Mod", s_hotkeyKeys[i]);
        int mod = (g_hotkeys[i].ctrl ? 1 : 0) | (g_hotkeys[i].shift ? 2 : 0) | (g_hotkeys[i].alt ? 4 : 0);
        swprintf(buf, MAX_PATH, L"%d", mod);
        WritePrivateProfileStringW(L"Hotkeys", modKey, buf, GetConfigPath());
    }
}
