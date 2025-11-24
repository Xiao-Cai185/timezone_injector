#include <windows.h>
#include <timezoneapi.h>
#include <string>
#include <stdlib.h>

// Global config - will be set via environment variables
long g_Bias = 0;
WCHAR g_StandardName[32] = L"Custom Standard Time";
WCHAR g_DaylightName[32] = L"Custom Daylight Time";
bool g_ConfigLoaded = false;

// Original function pointers
typedef DWORD (WINAPI *GetTimeZoneInformation_t)(LPTIME_ZONE_INFORMATION);
typedef DWORD (WINAPI *GetDynamicTimeZoneInformation_t)(PDYNAMIC_TIME_ZONE_INFORMATION);

GetTimeZoneInformation_t OriginalGetTimeZoneInformation = nullptr;
GetDynamicTimeZoneInformation_t OriginalGetDynamicTimeZoneInformation = nullptr;

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
        wcscpy_s(g_DaylightName, nameBuf);
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
    tz->DaylightBias = 0;
    wcscpy_s(tz->StandardName, g_StandardName);
    wcscpy_s(tz->DaylightName, g_DaylightName);
    
    // Clear transition times to disable DST
    memset(&tz->StandardDate, 0, sizeof(SYSTEMTIME));
    memset(&tz->DaylightDate, 0, sizeof(SYSTEMTIME));
}

void ApplyCustomDynamicTimeZone(DYNAMIC_TIME_ZONE_INFORMATION* tz)
{
    LoadConfig();
    tz->Bias = g_Bias;
    tz->StandardBias = 0;
    tz->DaylightBias = 0;
    wcscpy_s(tz->StandardName, g_StandardName);
    wcscpy_s(tz->DaylightName, g_DaylightName);
    
    // Clear transition times
    memset(&tz->StandardDate, 0, sizeof(SYSTEMTIME));
    memset(&tz->DaylightDate, 0, sizeof(SYSTEMTIME));
}

// Hooked functions
DWORD WINAPI HookedGetTimeZoneInformation(LPTIME_ZONE_INFORMATION tz)
{
    // Call original if available, otherwise use system default
    if (OriginalGetTimeZoneInformation) {
        OriginalGetTimeZoneInformation(tz);
    } else {
        GetTimeZoneInformation(tz);
    }
    
    ApplyCustomTimeZone(tz);
    return TIME_ZONE_ID_STANDARD;
}

DWORD WINAPI HookedGetDynamicTimeZoneInformation(PDYNAMIC_TIME_ZONE_INFORMATION tz)
{
    // Call original if available, otherwise use system default
    if (OriginalGetDynamicTimeZoneInformation) {
        OriginalGetDynamicTimeZoneInformation(tz);
    } else {
        GetDynamicTimeZoneInformation(tz);
    }
    
    ApplyCustomDynamicTimeZone(tz);
    return TIME_ZONE_ID_STANDARD;
}

// Simple IAT hooking
bool HookIAT(HMODULE hModule, const char* targetDll, const char* targetFunc, PVOID hookFunc, PVOID* originalFunc)
{
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return false;

    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hModule +
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    for (; importDesc->Name; importDesc++) {
        const char* dllName = (const char*)((BYTE*)hModule + importDesc->Name);
        if (_stricmp(dllName, targetDll) != 0) continue;

        PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)((BYTE*)hModule + importDesc->FirstThunk);
        PIMAGE_THUNK_DATA origThunk = (PIMAGE_THUNK_DATA)((BYTE*)hModule + importDesc->OriginalFirstThunk);

        for (; origThunk->u1.Function; thunk++, origThunk++) {
            if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) continue;

            PIMAGE_IMPORT_BY_NAME import = (PIMAGE_IMPORT_BY_NAME)((BYTE*)hModule + origThunk->u1.AddressOfData);
            if (strcmp((char*)import->Name, targetFunc) == 0) {
                DWORD oldProtect;
                VirtualProtect(&thunk->u1.Function, sizeof(PVOID), PAGE_READWRITE, &oldProtect);
                
                if (originalFunc) *originalFunc = (PVOID)thunk->u1.Function;
                thunk->u1.Function = (ULONG_PTR)hookFunc;
                
                VirtualProtect(&thunk->u1.Function, sizeof(PVOID), oldProtect, &oldProtect);
                return true;
            }
        }
    }
    return false;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        LoadConfig();

        // Hook the main executable's IAT
        HMODULE mainModule = GetModuleHandle(NULL);
        
        HookIAT(mainModule, "kernel32.dll", "GetTimeZoneInformation", 
                HookedGetTimeZoneInformation, (PVOID*)&OriginalGetTimeZoneInformation);
        
        HookIAT(mainModule, "kernel32.dll", "GetDynamicTimeZoneInformation", 
                HookedGetDynamicTimeZoneInformation, (PVOID*)&OriginalGetDynamicTimeZoneInformation);
    }
    return TRUE;
}
