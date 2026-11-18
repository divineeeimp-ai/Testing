// SLInjector.exe [process name] [dll path]
// Defaults: SCPSL.exe and SLBootstrap.dll next to this executable.
// Classic LoadLibraryW + CreateRemoteThread; success is confirmed by finding the
// module in the target's module list, not by the thread exit code.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cwchar>
#include <iterator>
#include <string>
#include <vector>

namespace
{
constexpr wchar_t kDefaultProcess[] = L"SCPSL.exe";
constexpr wchar_t kDefaultDll[] = L"SLBootstrap.dll";
constexpr DWORD kRemoteWaitMs = 15'000;

struct Handle
{
    HANDLE h = nullptr;
    Handle() = default;
    explicit Handle(HANDLE v) : h(v) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    ~Handle()
    {
        if (h && h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
    }
    explicit operator bool() const { return h && h != INVALID_HANDLE_VALUE; }
};

std::wstring ExeDirectory()
{
    std::vector<wchar_t> buf(32768);
    const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    std::wstring path(buf.data(), n);
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

std::wstring FileName(const std::wstring& path)
{
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::vector<DWORD> FindProcesses(const std::wstring& name)
{
    std::vector<DWORD> pids;
    Handle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap)
        return pids;
    PROCESSENTRY32W e{};
    e.dwSize = sizeof(e);
    for (BOOL ok = Process32FirstW(snap.h, &e); ok; ok = Process32NextW(snap.h, &e))
        if (_wcsicmp(e.szExeFile, name.c_str()) == 0)
            pids.push_back(e.th32ProcessID);
    return pids;
}

bool HasModule(DWORD pid, const std::wstring& moduleName)
{
    Handle snap(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid));
    if (!snap)
        return false;
    MODULEENTRY32W m{};
    m.dwSize = sizeof(m);
    for (BOOL ok = Module32FirstW(snap.h, &m); ok; ok = Module32NextW(snap.h, &m))
        if (_wcsicmp(m.szModule, moduleName.c_str()) == 0)
            return true;
    return false;
}

int Fail(const char* what)
{
    std::fprintf(stderr, "error: %s (GetLastError=%lu)\n", what, GetLastError());
    return 1;
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    const std::wstring processName = argc > 1 ? argv[1] : kDefaultProcess;
    const std::wstring dllArg = argc > 2 ? argv[2] : ExeDirectory() + L"\\" + kDefaultDll;

    wchar_t full[MAX_PATH * 4];
    const DWORD fullLen = GetFullPathNameW(dllArg.c_str(), static_cast<DWORD>(std::size(full)), full, nullptr);
    if (fullLen == 0 || fullLen >= std::size(full))
        return Fail("GetFullPathNameW");
    const std::wstring dllPath(full, fullLen);
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        std::fwprintf(stderr, L"error: %ls does not exist\n", dllPath.c_str());
        return 1;
    }

    const auto pids = FindProcesses(processName);
    if (pids.empty())
    {
        std::fwprintf(stderr, L"error: no running process named %ls\n", processName.c_str());
        return 1;
    }
    if (pids.size() > 1)
    {
        std::fwprintf(stderr, L"error: %zu processes named %ls; close the extras\n", pids.size(), processName.c_str());
        return 1;
    }
    const DWORD pid = pids.front();
    const std::wstring dllName = FileName(dllPath);
    if (HasModule(pid, dllName))
    {
        std::fwprintf(stderr, L"%ls is already loaded in pid %lu; restart the game to load it again\n", dllName.c_str(), pid);
        return 1;
    }

    Handle proc(OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                PROCESS_VM_WRITE | PROCESS_VM_READ,
                            FALSE, pid));
    if (!proc)
        return Fail("OpenProcess");

    BOOL wow64 = FALSE;
    if (!IsWow64Process(proc.h, &wow64))
        return Fail("IsWow64Process");
    if (wow64)
    {
        std::fprintf(stderr, "error: target is a 32-bit process; this build is x64 only\n");
        return 1;
    }

    const size_t bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(proc.h, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote)
        return Fail("VirtualAllocEx");

    int rc = 0;
    SIZE_T written = 0;
    if (!WriteProcessMemory(proc.h, remote, dllPath.c_str(), bytes, &written) || written != bytes)
    {
        rc = Fail("WriteProcessMemory");
    }
    else
    {
        // kernel32 is mapped at the same base in every process for the current boot.
        auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<void*>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")));
        if (!loadLibrary)
        {
            VirtualFreeEx(proc.h, remote, 0, MEM_RELEASE);
            return Fail("GetProcAddress(LoadLibraryW)");
        }
        Handle thread(CreateRemoteThread(proc.h, nullptr, 0, loadLibrary, remote, 0, nullptr));
        if (!thread)
        {
            rc = Fail("CreateRemoteThread");
        }
        else if (WaitForSingleObject(thread.h, kRemoteWaitMs) != WAIT_OBJECT_0)
        {
            // The thread may still be using the path buffer; leak it rather than free it under its feet.
            std::fprintf(stderr, "error: remote LoadLibraryW did not finish within %lu ms\n", kRemoteWaitMs);
            return 1;
        }
        else if (!HasModule(pid, dllName))
        {
            std::fwprintf(stderr, L"error: LoadLibraryW ran but %ls is not in the module list\n", dllName.c_str());
            rc = 1;
        }
        else
        {
            std::wprintf(L"loaded %ls into pid %lu\n", dllPath.c_str(), pid);
            std::wprintf(L"logs: SLBootstrap.log and SLInternal.log next to the DLL\n");
        }
    }

    VirtualFreeEx(proc.h, remote, 0, MEM_RELEASE);
    return rc;
}
