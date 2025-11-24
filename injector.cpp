#include <windows.h>
#include <string>

int main(int argc, char* argv[])
{
    if (argc != 3) {
        return 1;
    }

    const char* exe = argv[1];
    const char* dll = argv[2];

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION pi;

    if (!CreateProcessA(exe, NULL, NULL, NULL, FALSE,
        CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        return 1;
    }

    LPVOID remoteMem = VirtualAllocEx(pi.hProcess, NULL,
                                      strlen(dll) + 1,
                                      MEM_COMMIT, PAGE_READWRITE);

    WriteProcessMemory(pi.hProcess, remoteMem, dll,
                       strlen(dll) + 1, NULL);

    HANDLE hThread = CreateRemoteThread(
        pi.hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)LoadLibraryA,
        remoteMem,
        0,
        NULL
    );

    WaitForSingleObject(hThread, INFINITE);

    ResumeThread(pi.hThread);

    return 0;
}
