// SLInject: loads SLMenu.dll into a running SCPSL.exe with LoadLibraryW in a remote thread.
// Usage: SLInject.exe [path\to\SLMenu.dll] [process name, default SCPSL.exe]

#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cwchar>
#include <string>

namespace {

DWORD FindProcess(const wchar_t* exeName, int& matches)
{
    matches = 0;
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, exeName) == 0) {
            pid = pe.th32ProcessID;
            ++matches;
        }
    }
    CloseHandle(snap);
    return pid;
}

bool HasModule(DWORD pid, const wchar_t* moduleName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE)
        return false;
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    bool found = false;
    for (BOOL ok = Module32FirstW(snap, &me); ok && !found; ok = Module32NextW(snap, &me))
        found = _wcsicmp(me.szModule, moduleName) == 0;
    CloseHandle(snap);
    return found;
}

int Fail(const char* what)
{
    std::fprintf(stderr, "error: %s (GetLastError=%lu)\n", what, GetLastError());
    return 1;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    std::wstring dll = argc > 1 ? argv[1] : L"SLMenu.dll";
    const wchar_t* exe = argc > 2 ? argv[2] : L"SCPSL.exe";

    wchar_t full[MAX_PATH] = {};
    DWORD n = GetFullPathNameW(dll.c_str(), MAX_PATH, full, nullptr);
    if (n == 0 || n >= MAX_PATH)
        return Fail("bad DLL path");
    if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES)
        return Fail("DLL not found");

    int matches = 0;
    const DWORD pid = FindProcess(exe, matches);
    if (!pid) {
        std::fwprintf(stderr, L"error: %ls is not running\n", exe);
        return 1;
    }
    if (matches > 1) {
        std::fwprintf(stderr, L"error: %d processes named %ls; close the extra ones\n", matches, exe);
        return 1;
    }
    if (!HasModule(pid, L"GameAssembly.dll"))
        std::fwprintf(stderr, L"warning: GameAssembly.dll not loaded yet in pid %lu; SLMenu will wait for it\n", pid);

    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                     PROCESS_VM_WRITE | PROCESS_VM_READ,
                                 FALSE, pid);
    if (!process)
        return Fail("OpenProcess");

    BOOL wow64 = FALSE;
    if (IsWow64Process(process, &wow64) && wow64) {
        CloseHandle(process);
        std::fprintf(stderr, "error: target is 32-bit; SLMenu is x64\n");
        return 1;
    }

    const SIZE_T bytes = (wcslen(full) + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        CloseHandle(process);
        return Fail("VirtualAllocEx");
    }
    SIZE_T written = 0;
    if (!WriteProcessMemory(process, remote, full, bytes, &written) || written != bytes) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return Fail("WriteProcessMemory");
    }

    // kernel32 is mapped at the same address in every process of a boot session.
    auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")));
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary, remote, 0, nullptr);
    if (!thread) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return Fail("CreateRemoteThread");
    }
    WaitForSingleObject(thread, 30000);
    DWORD code = 0;
    GetExitCodeThread(thread, &code); // low 32 bits of the HMODULE; 0 means LoadLibraryW failed
    CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);

    const wchar_t* fileName = wcsrchr(full, L'\\');
    fileName = fileName ? fileName + 1 : full;
    if (!HasModule(pid, fileName)) {
        std::fprintf(stderr, "error: LoadLibraryW failed inside the game (thread exit 0x%08lX)\n", code);
        return 1;
    }
    std::wprintf(L"injected %ls into %ls (pid %lu). Log: SLMenu.log next to the DLL.\n", full, exe, pid);
    return 0;
}
