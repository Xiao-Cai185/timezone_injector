#include <windows.h>
#include <string>
#include <vector>
#include <psapi.h>

// 全局配置
long g_Bias = 0;
char g_TimeZoneId[128] = "Asia/Tokyo";
bool g_ConfigLoaded = false;

// ICU 时区相关的函数签名
typedef void* (*icu_TimeZone_createDefault_t)();
typedef void* (*icu_TimeZone_createTimeZone_t)(const char* id);

icu_TimeZone_createDefault_t Original_TimeZone_createDefault = nullptr;
void* g_CustomTimeZone = nullptr;

void LoadConfig()
{
    if (g_ConfigLoaded) return;

    char envBuf[64];
    if (GetEnvironmentVariableA("TZ_BIAS", envBuf, sizeof(envBuf))) {
        g_Bias = atol(envBuf);
    } else {
        g_Bias = -540; // 默认东京
    }

    // 从环境变量读取 IANA 时区 ID
    if (GetEnvironmentVariableA("TZ_IANA_ID", envBuf, sizeof(envBuf))) {
        strcpy_s(g_TimeZoneId, envBuf);
    } else {
        strcpy_s(g_TimeZoneId, "Asia/Tokyo");
    }

    g_ConfigLoaded = true;
}

// 查找 ICU 模块中的函数
FARPROC FindICUFunction(const char* funcPattern)
{
    HMODULE modules[1024];
    DWORD needed;
    
    if (EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed)) {
        for (unsigned int i = 0; i < (needed / sizeof(HMODULE)); i++) {
            char modName[MAX_PATH];
            if (GetModuleFileNameExA(GetCurrentProcess(), modules[i], modName, sizeof(modName))) {
                // 查找 ICU 相关的 DLL (chrome 通常是 icuuc.dll 或内嵌在主程序中)
                if (strstr(modName, "icuuc") || strstr(modName, "chrome.dll") || strstr(modName, "msedge.dll")) {
                    // 尝试获取导出函数
                    FARPROC proc = GetProcAddress(modules[i], funcPattern);
                    if (proc) return proc;
                }
            }
        }
    }
    return nullptr;
}

// Hook 的 TimeZone::createDefault 函数
void* WINAPI Hooked_TimeZone_createDefault()
{
    LoadConfig();
    
    // 如果已经创建了自定义时区对象，直接返回
    if (g_CustomTimeZone) {
        return g_CustomTimeZone;
    }

    // 尝试查找 createTimeZone 函数来创建自定义时区
    icu_TimeZone_createTimeZone_t createTimeZone = 
        (icu_TimeZone_createTimeZone_t)FindICUFunction("?createTimeZone@TimeZone@icu@@SAPEAV12@PEBD@Z");
    
    if (!createTimeZone) {
        // 尝试其他可能的符号
        createTimeZone = (icu_TimeZone_createTimeZone_t)FindICUFunction("icu::TimeZone::createTimeZone");
    }

    if (createTimeZone) {
        g_CustomTimeZone = createTimeZone(g_TimeZoneId);
        if (g_CustomTimeZone) {
            return g_CustomTimeZone;
        }
    }

    // 如果无法创建自定义时区，调用原始函数
    if (Original_TimeZone_createDefault) {
        return Original_TimeZone_createDefault();
    }

    return nullptr;
}

// 简单的 IAT Hook
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

// 内联 Hook (用于 ICU 函数)
bool InlineHook(PVOID targetFunc, PVOID hookFunc, PVOID* originalFunc)
{
    if (!targetFunc || !hookFunc) return false;

    DWORD oldProtect;
    if (!VirtualProtect(targetFunc, 14, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    // 保存原始字节
    BYTE* target = (BYTE*)targetFunc;
    
    // 写入跳转指令 (JMP [RIP+0]; 64-bit absolute jump)
    target[0] = 0xFF; // JMP
    target[1] = 0x25; // [RIP+0]
    *(DWORD*)(target + 2) = 0; // offset
    *(PVOID*)(target + 6) = hookFunc; // absolute address

    VirtualProtect(targetFunc, 14, oldProtect, &oldProtect);
    
    if (originalFunc) {
        *originalFunc = targetFunc;
    }

    return true;
}

// DLL 主入口
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        LoadConfig();

        // 等待一段时间让浏览器加载 ICU 模块
        Sleep(500);

        // 尝试查找并 Hook ICU TimeZone::createDefault
        FARPROC icuFunc = FindICUFunction("?createDefault@TimeZone@icu@@SAPEAV12@XZ");
        
        if (!icuFunc) {
            // 尝试其他可能的符号
            icuFunc = FindICUFunction("icu::TimeZone::createDefault");
        }

        if (icuFunc) {
            InlineHook((PVOID)icuFunc, Hooked_TimeZone_createDefault, (PVOID*)&Original_TimeZone_createDefault);
        }

        // 同时 Hook Windows 时区 API 作为后备
        HMODULE mainModule = GetModuleHandle(NULL);
        HookIAT(mainModule, "kernel32.dll", "GetTimeZoneInformation", 
                (PVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetTimeZoneInformation"), nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        // 清理
        if (g_CustomTimeZone) {
            // 注意：这里应该调用 ICU 的 delete 函数，但为了简化暂时省略
        }
    }
    
    return TRUE;
}
