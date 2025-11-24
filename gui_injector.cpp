#include <windows.h>
#include <commdlg.h>
#include <string>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "comdlg32.lib")

// Control IDs
#define ID_EDIT_TARGET 101
#define ID_BTN_BROWSE 102
#define ID_COMBO_TZ 103
#define ID_BTN_INJECT 104

HWND hEditTarget, hBtnBrowse, hComboTz, hBtnInject;

struct TimeZoneInfo {
    const wchar_t* name;
    long bias; // minutes (negative for east of UTC, positive for west)
};

// Comprehensive timezone list
std::vector<TimeZoneInfo> timezones = {
    { L"UTC (UTC+0)", 0 },
    { L"Tokyo (UTC+9)", -540 },
    { L"Seoul (UTC+9)", -540 },
    { L"Beijing/Shanghai (UTC+8)", -480 },
    { L"Hong Kong (UTC+8)", -480 },
    { L"Singapore (UTC+8)", -480 },
    { L"Bangkok (UTC+7)", -420 },
    { L"Mumbai (UTC+5:30)", -330 },
    { L"Dubai (UTC+4)", -240 },
    { L"Moscow (UTC+3)", -180 },
    { L"Istanbul (UTC+3)", -180 },
    { L"Paris/Berlin (UTC+1)", -60 },
    { L"London (UTC+0)", 0 },
    { L"New York (UTC-5)", 300 },
    { L"Chicago (UTC-6)", 360 },
    { L"Denver (UTC-7)", 420 },
    { L"Los Angeles (UTC-8)", 480 },
    { L"Sydney (UTC+10)", -600 },
    { L"Melbourne (UTC+10)", -600 },
    { L"Auckland (UTC+12)", -720 }
};

// DLL Injection Logic
bool Inject(const std::wstring& exePath, const std::wstring& dllPath, long bias, const std::wstring& tzName) {
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi;

    // Set environment variables for the current process so the child inherits them
    std::wstring biasStr = std::to_wstring(bias);
    SetEnvironmentVariableW(L"TZ_BIAS", biasStr.c_str());
    SetEnvironmentVariableW(L"TZ_NAME", tzName.c_str());

    // Create process in suspended state
    if (!CreateProcessW(exePath.c_str(), NULL, NULL, NULL, FALSE,
        CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        DWORD error = GetLastError();
        wchar_t errorMsg[512];
        swprintf_s(errorMsg, L"Failed to create process.\\n\\nError Code: %lu\\n\\nPlease check:\\n- File exists and is a valid executable\\n- You have permission to run it\\n- The file is not corrupted", error);
        MessageBoxW(NULL, errorMsg, L"Error", MB_ICONERROR);
        return false;
    }

    // Calculate DLL path length in bytes (including null terminator)
    size_t dllSize = (dllPath.length() + 1) * sizeof(wchar_t);
    
    // Allocate memory in remote process
    LPVOID remoteMem = VirtualAllocEx(pi.hProcess, NULL,
                                      dllSize,
                                      MEM_COMMIT, PAGE_READWRITE);

    if (!remoteMem) {
        DWORD error = GetLastError();
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        wchar_t errorMsg[256];
        swprintf_s(errorMsg, L"Failed to allocate memory in remote process.\\n\\nError Code: %lu", error);
        MessageBoxW(NULL, errorMsg, L"Error", MB_ICONERROR);
        return false;
    }

    // Write DLL path to remote process
    if (!WriteProcessMemory(pi.hProcess, remoteMem, dllPath.c_str(),
                       dllSize, NULL)) {
        DWORD error = GetLastError();
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        wchar_t errorMsg[256];
        swprintf_s(errorMsg, L"Failed to write memory in remote process.\\n\\nError Code: %lu", error);
        MessageBoxW(NULL, errorMsg, L"Error", MB_ICONERROR);
        return false;
    }

    // Create remote thread to load DLL
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
        DWORD error = GetLastError();
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        wchar_t errorMsg[512];
        swprintf_s(errorMsg, L"Failed to create remote thread.\\n\\nError Code: %lu\\n\\nThis may happen if:\\n- The target process has anti-injection protection\\n- You need Administrator privileges\\n- The DLL file is missing or invalid", error);
        MessageBoxW(NULL, errorMsg, L"Error", MB_ICONERROR);
        return false;
    }

    // Wait for DLL to load
    WaitForSingleObject(hThread, INFINITE);
    
    // Resume main thread
    ResumeThread(pi.hThread);
    
    // Cleanup
    CloseHandle(hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return true;
}

// Window Procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        // Target executable label and input
        CreateWindowW(L"STATIC", L"Target Executable:", WS_VISIBLE | WS_CHILD, 10, 10, 120, 20, hwnd, NULL, NULL, NULL);
        hEditTarget = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 10, 35, 300, 25, hwnd, (HMENU)ID_EDIT_TARGET, NULL, NULL);
        hBtnBrowse = CreateWindowW(L"BUTTON", L"Browse...", WS_VISIBLE | WS_CHILD, 320, 35, 80, 25, hwnd, (HMENU)ID_BTN_BROWSE, NULL, NULL);
        
        // Timezone selection label and dropdown
        CreateWindowW(L"STATIC", L"Select Timezone:", WS_VISIBLE | WS_CHILD, 10, 70, 120, 20, hwnd, NULL, NULL, NULL);
        hComboTz = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 10, 95, 390, 200, hwnd, (HMENU)ID_COMBO_TZ, NULL, NULL);
        
        // Populate timezone dropdown
        for (const auto& tz : timezones) {
            SendMessageW(hComboTz, CB_ADDSTRING, 0, (LPARAM)tz.name);
        }
        SendMessageW(hComboTz, CB_SETCURSEL, 1, 0); // Default to Tokyo
        
        // Inject button
        hBtnInject = CreateWindowW(L"BUTTON", L"Launch && Inject", WS_VISIBLE | WS_CHILD, 10, 140, 390, 40, hwnd, (HMENU)ID_BTN_INJECT, NULL, NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BTN_BROWSE) {
            // Browse for executable
            wchar_t filename[MAX_PATH] = {0};
            OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"Executables (*.exe)\\0*.exe\\0All Files (*.*)\\0*.*\\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            
            if (GetOpenFileNameW(&ofn)) {
                SetWindowTextW(hEditTarget, filename);
            }
        }
        else if (LOWORD(wParam) == ID_BTN_INJECT) {
            // Get target executable path
            wchar_t exePath[MAX_PATH];
            GetWindowTextW(hEditTarget, exePath, MAX_PATH);
            
            if (wcslen(exePath) == 0) {
                MessageBoxW(hwnd, L"Please select a target executable.", L"Error", MB_ICONERROR);
                break;
            }

            // Get selected timezone
            int idx = SendMessageW(hComboTz, CB_GETCURSEL, 0, 0);
            if (idx == CB_ERR) idx = 0;
            
            // Get DLL path (assume in same directory as this executable)
            wchar_t dllPath[MAX_PATH];
            GetModuleFileNameW(NULL, dllPath, MAX_PATH);
            std::wstring pathStr = dllPath;
            pathStr = pathStr.substr(0, pathStr.find_last_of(L"\\/"));
            pathStr += L"\\timezonehook.dll";

            // Check if DLL exists
            DWORD fileAttr = GetFileAttributesW(pathStr.c_str());
            if (fileAttr == INVALID_FILE_ATTRIBUTES) {
                wchar_t errorMsg[512];
                swprintf_s(errorMsg, L"DLL file not found!\\n\\nExpected location:\\n%s\\n\\nPlease ensure timezonehook.dll is in the same directory as this executable.", pathStr.c_str());
                MessageBoxW(hwnd, errorMsg, L"Error", MB_ICONERROR);
                break;
            }

            // Perform injection
            if (Inject(exePath, pathStr, timezones[idx].bias, timezones[idx].name)) {
                MessageBoxW(hwnd, L"Launched and Injected successfully!\\n\\nThe target application is now running with the selected timezone.", L"Success", MB_ICONINFORMATION);
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Application Entry Point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"TimezoneInjectorClass";
    
    // Register window class
    WNDCLASSW wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    // Create window
    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Timezone Injector",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 430, 240,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    // Message loop
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
