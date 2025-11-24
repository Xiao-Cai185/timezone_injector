#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <map>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")

// 控件 ID
#define ID_EDIT_TARGET 101
#define ID_BTN_BROWSE 102
#define ID_COMBO_TZ 103
#define ID_BTN_INJECT 104
#define ID_CHECK_BROWSER 105
#define ID_COMBO_LANG 106
#define ID_STATIC_TARGET 107
#define ID_STATIC_TZ 108
#define ID_STATIC_LANG 109

// 颜色定义（淡蓝色主题）
#define COLOR_BG RGB(240, 248, 255)        // Alice Blue
#define COLOR_PANEL RGB(230, 240, 250)     // Light Blue
#define COLOR_BUTTON RGB(100, 149, 237)    // Cornflower Blue
#define COLOR_BUTTON_HOVER RGB(65, 105, 225) // Royal Blue
#define COLOR_TEXT RGB(25, 25, 112)        // Midnight Blue
#define COLOR_BORDER RGB(176, 196, 222)    // Light Steel Blue

// 全局变量
HWND hEditTarget, hBtnBrowse, hComboTz, hBtnInject, hCheckBrowser, hComboLang;
HWND hStaticTarget, hStaticTz, hStaticLang;
HFONT hFont;
HBRUSH hBrushBg, hBrushPanel, hBrushButton;
bool isHoveringButton = false;

// 语言枚举
enum Language { LANG_CN, LANG_EN };
Language currentLang = LANG_CN;

// 多语言文本
struct UIText {
    const wchar_t* cn;
    const wchar_t* en;
};

std::map<std::string, UIText> uiTexts = {
    {"title", {L"时区注入器", L"Timezone Injector"}},
    {"target", {L"目标程序：", L"Target Executable:"}},
    {"browse", {L"浏览...", L"Browse..."}},
    {"timezone", {L"选择时区：", L"Select Timezone:"}},
    {"browser_mode", {L"浏览器模式（Hook ICU）", L"Browser Mode (Hook ICU)"}},
    {"language", {L"语言：", L"Language:"}},
    {"inject", {L"启动并注入", L"Launch && Inject"}},
    {"success", {L"成功", L"Success"}},
    {"error", {L"错误", L"Error"}},
    {"success_msg", {L"启动并注入成功！\n\n目标应用程序现在正以选定的时区运行。", L"Launched and Injected successfully!\n\nThe target application is now running with the selected timezone."}},
    {"select_exe", {L"请选择目标可执行文件。", L"Please select a target executable."}},
    {"dll_not_found", {L"找不到 DLL 文件！\n\n预期位置：\n%s\n\n请确保 DLL 文件与本程序在同一目录。", L"DLL file not found!\n\nExpected location:\n%s\n\nPlease ensure the DLL is in the same directory as this executable."}},
    {"lang_cn", {L"中文", L"Chinese"}},
    {"lang_en", {L"English", L"English"}}
};

const wchar_t* GetText(const char* key) {
    auto it = uiTexts.find(key);
    if (it != uiTexts.end()) {
        return (currentLang == LANG_CN) ? it->second.cn : it->second.en;
    }
    return L"";
}

struct TimeZoneInfo {
    const wchar_t* name_cn;
    const wchar_t* name_en;
    const char* iana_id;  // IANA 时区 ID（用于浏览器）
    long bias;
};

std::vector<TimeZoneInfo> timezones = {
    { L"UTC (UTC+0)", L"UTC (UTC+0)", "UTC", 0 },
    { L"东京 (UTC+9)", L"Tokyo (UTC+9)", "Asia/Tokyo", -540 },
    { L"首尔 (UTC+9)", L"Seoul (UTC+9)", "Asia/Seoul", -540 },
    { L"北京/上海 (UTC+8)", L"Beijing/Shanghai (UTC+8)", "Asia/Shanghai", -480 },
    { L"香港 (UTC+8)", L"Hong Kong (UTC+8)", "Asia/Hong_Kong", -480 },
    { L"新加坡 (UTC+8)", L"Singapore (UTC+8)", "Asia/Singapore", -480 },
    { L"曼谷 (UTC+7)", L"Bangkok (UTC+7)", "Asia/Bangkok", -420 },
    { L"孟买 (UTC+5:30)", L"Mumbai (UTC+5:30)", "Asia/Kolkata", -330 },
    { L"迪拜 (UTC+4)", L"Dubai (UTC+4)", "Asia/Dubai", -240 },
    { L"莫斯科 (UTC+3)", L"Moscow (UTC+3)", "Europe/Moscow", -180 },
    { L"伊斯坦布尔 (UTC+3)", L"Istanbul (UTC+3)", "Europe/Istanbul", -180 },
    { L"巴黎/柏林 (UTC+1)", L"Paris/Berlin (UTC+1)", "Europe/Paris", -60 },
    { L"伦敦 (UTC+0)", L"London (UTC+0)", "Europe/London", 0 },
    { L"纽约 (UTC-5)", L"New York (UTC-5)", "America/New_York", 300 },
    { L"芝加哥 (UTC-6)", L"Chicago (UTC-6)", "America/Chicago", 360 },
    { L"丹佛 (UTC-7)", L"Denver (UTC-7)", "America/Denver", 420 },
    { L"洛杉矶 (UTC-8)", L"Los Angeles (UTC-8)", "America/Los_Angeles", 480 },
    { L"悉尼 (UTC+10)", L"Sydney (UTC+10)", "Australia/Sydney", -600 },
    { L"墨尔本 (UTC+10)", L"Melbourne (UTC+10)", "Australia/Melbourne", -600 },
    { L"奥克兰 (UTC+12)", L"Auckland (UTC+12)", "Pacific/Auckland", -720 }
};

void UpdateLanguage() {
    SetWindowTextW(GetParent(hStaticTarget), GetText("title"));
    SetWindowTextW(hStaticTarget, GetText("target"));
    SetWindowTextW(hBtnBrowse, GetText("browse"));
    SetWindowTextW(hStaticTz, GetText("timezone"));
    SetWindowTextW(hCheckBrowser, GetText("browser_mode"));
    SetWindowTextW(hStaticLang, GetText("language"));
    SetWindowTextW(hBtnInject, GetText("inject"));
    
    // 更新时区列表
    int currentSel = SendMessageW(hComboTz, CB_GETCURSEL, 0, 0);
    SendMessageW(hComboTz, CB_RESETCONTENT, 0, 0);
    for (const auto& tz : timezones) {
        const wchar_t* name = (currentLang == LANG_CN) ? tz.name_cn : tz.name_en;
        SendMessageW(hComboTz, CB_ADDSTRING, 0, (LPARAM)name);
    }
    if (currentSel >= 0 && currentSel < (int)timezones.size()) {
        SendMessageW(hComboTz, CB_SETCURSEL, currentSel, 0);
    } else {
        SendMessageW(hComboTz, CB_SETCURSEL, 1, 0);
    }
}

// DLL 注入逻辑
bool Inject(const std::wstring& exePath, const std::wstring& dllPath, long bias, const std::wstring& tzName, const char* ianaId) {
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi;

    // 设置环境变量
    std::wstring biasStr = std::to_wstring(bias);
    SetEnvironmentVariableW(L"TZ_BIAS", biasStr.c_str());
    SetEnvironmentVariableW(L"TZ_NAME", tzName.c_str());
    
    // 设置 IANA 时区 ID（用于浏览器）
    if (ianaId) {
        SetEnvironmentVariableA("TZ_IANA_ID", ianaId);
    }

    if (!CreateProcessW(exePath.c_str(), NULL, NULL, NULL, FALSE,
        CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        DWORD error = GetLastError();
        wchar_t errorMsg[512];
        swprintf_s(errorMsg, L"Failed to create process.\n\nError Code: %lu\n\nPlease check:\n- File exists and is a valid executable\n- You have permission to run it\n- The file is not corrupted", error);
        MessageBoxW(NULL, errorMsg, GetText("error"), MB_ICONERROR);
        return false;
    }

    size_t dllSize = (dllPath.length() + 1) * sizeof(wchar_t);
    
    LPVOID remoteMem = VirtualAllocEx(pi.hProcess, NULL, dllSize, MEM_COMMIT, PAGE_READWRITE);

    if (!remoteMem) {
        DWORD error = GetLastError();
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        wchar_t errorMsg[256];
        swprintf_s(errorMsg, L"Failed to allocate memory in remote process.\n\nError Code: %lu", error);
        MessageBoxW(NULL, errorMsg, GetText("error"), MB_ICONERROR);
        return false;
    }

    if (!WriteProcessMemory(pi.hProcess, remoteMem, dllPath.c_str(), dllSize, NULL)) {
        DWORD error = GetLastError();
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        wchar_t errorMsg[256];
        swprintf_s(errorMsg, L"Failed to write memory in remote process.\n\nError Code: %lu", error);
        MessageBoxW(NULL, errorMsg, GetText("error"), MB_ICONERROR);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(pi.hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)LoadLibraryW, remoteMem, 0, NULL);

    if (!hThread) {
        DWORD error = GetLastError();
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        wchar_t errorMsg[512];
        swprintf_s(errorMsg, L"Failed to create remote thread.\n\nError Code: %lu\n\nThis may happen if:\n- The target process has anti-injection protection\n- You need Administrator privileges\n- The DLL file is missing or invalid", error);
        MessageBoxW(NULL, errorMsg, GetText("error"), MB_ICONERROR);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    ResumeThread(pi.hThread);
    
    CloseHandle(hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return true;
}

// 自定义按钮绘制
void DrawCustomButton(HDC hdc, RECT* rect, const wchar_t* text, bool isHover) {
    // 绘制按钮背景
    HBRUSH brush = CreateSolidBrush(isHover ? COLOR_BUTTON_HOVER : COLOR_BUTTON);
    FillRect(hdc, rect, brush);
    DeleteObject(brush);
    
    // 绘制边框
    HPEN pen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    Rectangle(hdc, rect->left, rect->top, rect->right, rect->bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    
    // 绘制文本
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    SelectObject(hdc, hFont);
    DrawTextW(hdc, text, -1, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// 窗口过程
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
    {
        // 创建字体（微软雅黑）
        hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
        
        // 创建画刷
        hBrushBg = CreateSolidBrush(COLOR_BG);
        hBrushPanel = CreateSolidBrush(COLOR_PANEL);
        hBrushButton = CreateSolidBrush(COLOR_BUTTON);
        
        // 语言选择
        hStaticLang = CreateWindowW(L"STATIC", GetText("language"), 
            WS_VISIBLE | WS_CHILD, 10, 10, 80, 25, hwnd, (HMENU)ID_STATIC_LANG, NULL, NULL);
        hComboLang = CreateWindowW(L"COMBOBOX", L"", 
            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, 100, 10, 120, 100, hwnd, (HMENU)ID_COMBO_LANG, NULL, NULL);
        SendMessageW(hComboLang, CB_ADDSTRING, 0, (LPARAM)GetText("lang_cn"));
        SendMessageW(hComboLang, CB_ADDSTRING, 0, (LPARAM)GetText("lang_en"));
        SendMessageW(hComboLang, CB_SETCURSEL, 0, 0);
        
        // 目标程序
        hStaticTarget = CreateWindowW(L"STATIC", GetText("target"), 
            WS_VISIBLE | WS_CHILD, 10, 50, 120, 25, hwnd, (HMENU)ID_STATIC_TARGET, NULL, NULL);
        hEditTarget = CreateWindowW(L"EDIT", L"", 
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 10, 80, 380, 30, hwnd, (HMENU)ID_EDIT_TARGET, NULL, NULL);
        hBtnBrowse = CreateWindowW(L"BUTTON", GetText("browse"), 
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 400, 80, 100, 30, hwnd, (HMENU)ID_BTN_BROWSE, NULL, NULL);
        
        // 时区选择
        hStaticTz = CreateWindowW(L"STATIC", GetText("timezone"), 
            WS_VISIBLE | WS_CHILD, 10, 125, 120, 25, hwnd, (HMENU)ID_STATIC_TZ, NULL, NULL);
        hComboTz = CreateWindowW(L"COMBOBOX", L"", 
            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 10, 155, 490, 200, hwnd, (HMENU)ID_COMBO_TZ, NULL, NULL);
        
        // 浏览器模式复选框
        hCheckBrowser = CreateWindowW(L"BUTTON", GetText("browser_mode"), 
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 10, 195, 300, 25, hwnd, (HMENU)ID_CHECK_BROWSER, NULL, NULL);
        
        // 注入按钮
        hBtnInject = CreateWindowW(L"BUTTON", GetText("inject"), 
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 10, 235, 490, 45, hwnd, (HMENU)ID_BTN_INJECT, NULL, NULL);
        
        // 设置字体
        SendMessageW(hStaticLang, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hComboLang, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hStaticTarget, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hEditTarget, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hStaticTz, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hComboTz, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hCheckBrowser, WM_SETFONT, (WPARAM)hFont, TRUE);
        
        UpdateLanguage();
        break;
    }
    
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, COLOR_TEXT);
        SetBkColor(hdcStatic, COLOR_BG);
        return (LRESULT)hBrushBg;
    }
    
    case WM_CTLCOLOREDIT:
    {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, COLOR_TEXT);
        SetBkColor(hdcEdit, RGB(255, 255, 255));
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    }
    
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlID == ID_BTN_BROWSE || dis->CtlID == ID_BTN_INJECT) {
            wchar_t text[256];
            GetWindowTextW(dis->hwndItem, text, 256);
            bool isHover = (dis->itemState & ODS_SELECTED) || isHoveringButton;
            DrawCustomButton(dis->hDC, &dis->rcItem, text, isHover);
        }
        return TRUE;
    }
    
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_COMBO_LANG && HIWORD(wParam) == CBN_SELCHANGE) {
            int sel = SendMessageW(hComboLang, CB_GETCURSEL, 0, 0);
            currentLang = (sel == 0) ? LANG_CN : LANG_EN;
            UpdateLanguage();
        }
        else if (LOWORD(wParam) == ID_BTN_BROWSE) {
            wchar_t filename[MAX_PATH] = {0};
            OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"Executables (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            
            if (GetOpenFileNameW(&ofn)) {
                SetWindowTextW(hEditTarget, filename);
            }
        }
        else if (LOWORD(wParam) == ID_BTN_INJECT) {
            wchar_t exePath[MAX_PATH];
            GetWindowTextW(hEditTarget, exePath, MAX_PATH);
            
            if (wcslen(exePath) == 0) {
                MessageBoxW(hwnd, GetText("select_exe"), GetText("error"), MB_ICONERROR);
                break;
            }

            int idx = SendMessageW(hComboTz, CB_GETCURSEL, 0, 0);
            if (idx == CB_ERR) idx = 0;
            
            // 判断是否使用浏览器模式
            bool browserMode = (SendMessageW(hCheckBrowser, BM_GETCHECK, 0, 0) == BST_CHECKED);
            
            // 获取 DLL 路径
            wchar_t dllPath[MAX_PATH];
            GetModuleFileNameW(NULL, dllPath, MAX_PATH);
            std::wstring pathStr = dllPath;
            pathStr = pathStr.substr(0, pathStr.find_last_of(L"\\/"));
            
            if (browserMode) {
                pathStr += L"\\browserhook.dll";
            } else {
                pathStr += L"\\timezonehook.dll";
            }

            // 检查 DLL 是否存在
            DWORD fileAttr = GetFileAttributesW(pathStr.c_str());
            if (fileAttr == INVALID_FILE_ATTRIBUTES) {
                wchar_t errorMsg[512];
                swprintf_s(errorMsg, GetText("dll_not_found"), pathStr.c_str());
                MessageBoxW(hwnd, errorMsg, GetText("error"), MB_ICONERROR);
                break;
            }

            // 执行注入
            const wchar_t* tzName = (currentLang == LANG_CN) ? timezones[idx].name_cn : timezones[idx].name_en;
            if (Inject(exePath, pathStr, timezones[idx].bias, tzName, timezones[idx].iana_id)) {
                MessageBoxW(hwnd, GetText("success_msg"), GetText("success"), MB_ICONINFORMATION);
            }
        }
        break;

    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect(hdc, &rect, hBrushBg);
        return 1;
    }

    case WM_DESTROY:
        DeleteObject(hFont);
        DeleteObject(hBrushBg);
        DeleteObject(hBrushPanel);
        DeleteObject(hBrushButton);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// 程序入口
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 初始化 Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
    
    const wchar_t CLASS_NAME[] = L"TimezoneInjectorClass";
    
    WNDCLASSW wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(COLOR_BG);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, GetText("title"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 540, 350,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
