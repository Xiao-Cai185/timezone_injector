#include <windows.h>
#include <detours.h>
#include <timezoneapi.h>
#include <string>
#include <stdlib.h>

#pragma comment(lib, "detours.lib")

typedef DWORD (WINAPI *GetTimeZoneInformation_t)(LPTIME_ZONE_INFORMATION);
GetTimeZoneInformation_t TrueGetTimeZoneInformation;

typedef DWORD (WINAPI *GetDynamicTimeZoneInformation_t)(PDYNAMIC_TIME_ZONE_INFORMATION);
GetDynamicTimeZoneInformation_t TrueGetDynamicTimeZoneInformation;

// Global config
long g_Bias = 0;
WCHAR g_StandardName[32] = L"Custom Standard Time";
WCHAR g_DaylightName[32] = L"Custom Daylight Time";
bool g_ConfigLoaded = false;

void LoadConfig()
{
    if (g_ConfigLoaded) return;

    char envBuf[64];
    if (GetEnvironmentVariableA("TZ_BIAS", envBuf, sizeof(envBuf))) {
        g_Bias = atol(envBuf);
    } else {
        g_Bias = -540; // Default to Tokyo if not set
    }

    WCHAR nameBuf[64];
    if (GetEnvironmentVariableW(L"TZ_NAME", nameBuf, sizeof(nameBuf)/sizeof(WCHAR))) {
        wcscpy_s(g_StandardName, nameBuf);
        wcscpy_s(g_DaylightName, nameBuf); // Use same name for simplicity
    } else {
        wcscpy_s(g_StandardName, L"Tokyo Standard Time");
        wcscpy_s(g_DaylightName, L"Tokyo Daylight Time");
    }

    g_ConfigLoaded = true;
}

void ApplyCustomTimeZone(TIME_ZONE_INFORMATION* tz)
{
    LoadConfig();
    tz->Bias = g_Bias;
    tz->StandardBias = 0;
    tz->DaylightBias = 0; // Disable DST effect for simplicity or let user configure
    wcscpy_s(tz->StandardName, g_StandardName);
    wcscpy_s(tz->DaylightName, g_DaylightName);
}

void ApplyCustomDynamicTimeZone(DYNAMIC_TIME_ZONE_INFORMATION* tz)
{
    LoadConfig();
    tz->Bias = g_Bias;
    tz->StandardBias = 0;
    tz->DaylightBias = 0;
    wcscpy_s(tz->StandardName, g_StandardName);
    wcscpy_s(tz->DaylightName, g_DaylightName);
    // Dynamic specific fields could be handled here if needed
}

DWORD WINAPI HookedGetTimeZoneInformation(LPTIME_ZONE_INFORMATION tz)
{
    DWORD ret = TrueGetTimeZoneInformation(tz);
    ApplyCustomTimeZone(tz);
    return TIME_ZONE_ID_STANDARD; // Force standard time
}

DWORD WINAPI HookedGetDynamicTimeZoneInformation(PDYNAMIC_TIME_ZONE_INFORMATION tz)
{
    DWORD ret = TrueGetDynamicTimeZoneInformation(tz);
    ApplyCustomDynamicTimeZone(tz);
    return TIME_ZONE_ID_STANDARD;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        LoadConfig(); // Load config immediately

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        TrueGetTimeZoneInformation =
            (GetTimeZoneInformation_t)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "GetTimeZoneInformation");

        TrueGetDynamicTimeZoneInformation =
            (GetDynamicTimeZoneInformation_t)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "GetDynamicTimeZoneInformation");

        DetourAttach(&(PVOID&)TrueGetTimeZoneInformation, HookedGetTimeZoneInformation);
        DetourAttach(&(PVOID&)TrueGetDynamicTimeZoneInformation, HookedGetDynamicTimeZoneInformation);

        DetourTransactionCommit();
    }
    return TRUE;
}
