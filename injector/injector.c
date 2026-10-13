# Injector helper (simple) - build: cl /O2 injector.c /link kernel32.lib

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

static DWORD find_pid_by_name(const char *name) {
    PROCESSENTRY32 pe;
    HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hsnap == INVALID_HANDLE_VALUE) return 0;
    pe.dwSize = sizeof(pe);
    if (Process32First(hsnap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, name) == 0) {
                CloseHandle(hsnap);
                return pe.th32ProcessID;
            }
        } while (Process32Next(hsnap, &pe));
    }
    CloseHandle(hsnap);
    return 0;
}

int inject_dll(DWORD pid, const char *dll_path) {
    HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!hProc) return -1;
    size_t len = strlen(dll_path) + 1;
    LPVOID remote = VirtualAllocEx(hProc, NULL, len, MEM_COMMIT, PAGE_READWRITE);
    if (!remote) { CloseHandle(hProc); return -2; }
    if (!WriteProcessMemory(hProc, remote, dll_path, len, NULL)) { VirtualFreeEx(hProc, remote, 0, MEM_RELEASE); CloseHandle(hProc); return -3; }
    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    FARPROC pLoad = GetProcAddress(hKernel, "LoadLibraryA");
    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)pLoad, remote, 0, NULL);
    if (!hThread) { VirtualFreeEx(hProc, remote, 0, MEM_RELEASE); CloseHandle(hProc); return -4; }
    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: injector.exe <pid|procname> <path_to_present_hook.dll>\n");
        return 1;
    }
    DWORD pid = 0;
    if (isdigit(argv[1][0])) pid = (DWORD)atoi(argv[1]);
    else pid = find_pid_by_name(argv[1]);
    if (!pid) { printf("Could not find pid for %s\n", argv[1]); return 2; }
    int r = inject_dll(pid, argv[2]);
    if (r == 0) printf("Injected into %u\n", pid);
    else printf("Injection failed: %d\n", r);
    return 0;
}
