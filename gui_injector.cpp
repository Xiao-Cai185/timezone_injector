#include <windows.h>
#include <commdlg.h>
#include <string>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "comdlg32.lib")

// IDs
#define ID_EDIT_TARGET 101
#define ID_BTN_BROWSE 102
#define ID_COMBO_TZ 103
#define ID_BTN_INJECT 104

HWND hEditTarget, hBtnBrowse, hComboTz, hBtnInject;

struct TimeZoneInfo {
    const wchar_t* name;
    long bias; // minutes
};

std::vector<TimeZoneInfo> timezones = {
    { L"UTC", 0 },
    { L"Tokyo (UTC+9)", -540 },
    { L"Beijing (UTC+8)", -480 },
    { L"London (UTC+0)", 0 },
    { L"New York (UTC-5)", 300 },
    { L"Los Angeles (UTC-8)", 480 },
    { L"Paris (UTC+1)", -60 },
    { L"Moscow (UTC+3)", -180 },
    { L"Sydney (UTC+11)", -660 }
};

// Injection Logic
bool Inject(const std::wstring& exePath, const std::wstring& dllPath, long bias, const std::wstring& tzName) {
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi;

    // Set environment variables for the current process so the child inherits them
    std::wstring biasStr = std::to_wstring(bias);
    SetEnvironmentVariableW(L"TZ_BIAS", biasStr.c_str());
    SetEnvironmentVariableW(L"TZ_NAME", tzName.c_str());

    if (!CreateProcessW(exePath.c_str(), NULL, NULL, NULL, FALSE,
        CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        MessageBoxW(NULL, L"Failed to create process", L"Error", MB_ICONERROR);
        return false;
    }

    // Calculate DLL path length in bytes (including null terminator)
    size_t dllSize = (dllPath.length() + 1) * sizeof(wchar_t);
    
    LPVOID remoteMem = VirtualAllocEx(pi.hProcess, NULL,
                                      dllSize,
                                      MEM_COMMIT, PAGE_READWRITE);

    if (!remoteMem) {
        TerminateProcess(pi.hProcess, 1);
        MessageBoxW(NULL, L"Failed to allocate memory in remote process", L"Error", MB_ICONERROR);
        return false;
    }

    if (!WriteProcessMemory(pi.hProcess, remoteMem, dllPath.c_str(),
                       dllSize, NULL)) {
        TerminateProcess(pi.hProcess, 1);
        MessageBoxW(NULL, L"Failed to write memory in remote process", L"Error", MB_ICONERROR);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(
        pi.hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)LoadLibraryW, // Use LoadLibraryW for wide strings
        remoteMem,
        0,
        NULL
    );

    if (!hThread) {
        TerminateProcess(pi.hProcess, 1);
        MessageBoxW(NULL, L"Failed to create remote thread", L"Error", MB_ICONERROR);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    ResumeThread(pi.hThread);
    
    CloseHandle(hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return true;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        CreateWindowW(L"STATIC", L"Target Executable:", WS_VISIBLE | WS_CHILD, 10, 10, 120, 20, hwnd, NULL, NULL, NULL);
        hEditTarget = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 10, 35, 300, 25, hwnd, (HMENU)ID_EDIT_TARGET, NULL, NULL);
        hBtnBrowse = CreateWindowW(L"BUTTON", L"Browse...", WS_VISIBLE | WS_CHILD, 320, 35, 80, 25, hwnd, (HMENU)ID_BTN_BROWSE, NULL, NULL);
        
        CreateWindowW(L"STATIC", L"Select Timezone:", WS_VISIBLE | WS_CHILD, 10, 70, 120, 20, hwnd, NULL, NULL, NULL);
        hComboTz = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 10, 95, 300, 200, hwnd, (HMENU)ID_COMBO_TZ, NULL, NULL);
        
        for (const auto& tz : timezones) {
            SendMessageW(hComboTz, CB_ADDSTRING, 0, (LPARAM)tz.name);
        }
        SendMessageW(hComboTz, CB_SETCURSEL, 1, 0); // Default to Tokyo

        hBtnInject = CreateWindowW(L"BUTTON", L"Launch & Inject", WS_VISIBLE | WS_CHILD, 10, 140, 390, 40, hwnd, (HMENU)ID_BTN_INJECT, NULL, NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BTN_BROWSE) {
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
                MessageBoxW(hwnd, L"Please select a target executable.", L"Error", MB_ICONERROR);
                break;
            }

            int idx = SendMessageW(hComboTz, CB_GETCURSEL, 0, 0);
            if (idx == CB_ERR) idx = 0;
            
            // Get DLL path (assume in same dir as this exe)
            wchar_t dllPath[MAX_PATH];
            GetModuleFileNameW(NULL, dllPath, MAX_PATH);
            std::wstring pathStr = dllPath;
            pathStr = pathStr.substr(0, pathStr.find_last_of(L"\\/"));
            pathStr += L"\\timezonehook.dll";

            if (Inject(exePath, pathStr, timezones[idx].bias, timezones[idx].name)) {
                MessageBoxW(hwnd, L"Launched and Injected successfully!", L"Success", MB_OK);
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"TimezoneInjectorClass";
    
    WNDCLASSW wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Timezone Injector",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 430, 240,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
